#include "player/my_player.hpp"

#include <algorithm>
#include <cstdlib>

namespace ttt::my_player
{

  namespace
  {
    constexpr std::int8_t EMPTY = 0, ME = 1, OPP = 2, WALL = 3;
    constexpr int DX[4] = {1, 0, 1, 1};
    constexpr int DY[4] = {0, 1, 1, -1};
    constexpr int INF = 1000000000;
    constexpr int WIN = 100000000;
    constexpr int ROOT_BEAM = 30; // сколько ходов рассматриваем в корне
    constexpr int BEAM = 12;      // ... и на остальных уровнях
    constexpr int MAX_DEPTH = 16;

    long long pow10ll(int n)
    {
      long long r = 1;
      while (n-- > 0)
        r *= 10;
      return r;
    }
    std::int8_t other(std::int8_t s) { return s == ME ? OPP : ME; }
  } // namespace

  void MyPlayer::set_sign(Sign sign) { m_sign = sign; }

  const char *MyPlayer::get_name() const { return m_name; }

  std::int8_t MyPlayer::at(int x, int y) const
  {
    if (x < 0 || y < 0 || x >= m_nx || y >= m_ny)
      return WALL;
    return m_b[x * m_ny + y];
  }

  // Копируем поле из игры во внутреннее представление и считаем оценку.
  void MyPlayer::load(const State &game, Sign me)
  {
    const auto &o = game.get_opts();
    m_nx = o.rows;
    m_ny = o.cols;
    m_len = std::min(std::max(o.win_len, 2), 15);
    m_b.assign(m_nx * m_ny, EMPTY);
    m_near.assign(m_nx * m_ny, 0);
    m_score = 0;
    m_cands.assign(MAX_DEPTH + 2, std::vector<Cand>());

    // Таблицы весов.
    m_W[0] = m_att[0] = m_def[0] = 0;
    for (int c = 1; c < m_len; ++c)
    {
      m_W[c] = pow10ll(c - 1);
      m_att[c] = pow10ll(c);
      m_def[c] = pow10ll(c) / 2;
    }
    if (m_len >= 4)
    {
      m_W[m_len - 1] = 20000; // четверка: следующим ходом выигрыш
      m_W[m_len - 2] = 300;   // тройка: угроза создать четверку
    }
    m_W[m_len] = 10000000;
    m_att[m_len - 1] = 10000000;
    m_def[m_len - 1] = 1000000;

    for (int x = 0; x < m_nx; ++x)
      for (int y = 0; y < m_ny; ++y)
      {
        Sign s = game.get_value(x, y);
        std::int8_t v = EMPTY;
        if (s == Sign::WALL)
          v = WALL;
        else if (s == Sign::NONE)
          v = EMPTY;
        else
          v = (s == me) ? ME : OPP;
        m_b[x * m_ny + y] = v;
      }
    for (int x = 0; x < m_nx; ++x)
      for (int y = 0; y < m_ny; ++y)
        if (m_b[x * m_ny + y] == ME || m_b[x * m_ny + y] == OPP)
          bump_near(x, y, +1);
    // Полная начальная оценка: сумма по всем окнам длины win_len.
    for (int x = 0; x < m_nx; ++x)
      for (int y = 0; y < m_ny; ++y)
        for (int d = 0; d < 4; ++d)
          m_score += window_value(x, y, d);
  }

  // Оценка одного окна из m_len клеток: >0 в пользу меня, <0 в пользу соперника.
  long long MyPlayer::window_value(int sx, int sy, int d) const
  {
    int me = 0, opp = 0;
    for (int i = 0; i < m_len; ++i)
    {
      std::int8_t c = at(sx + DX[d] * i, sy + DY[d] * i);
      if (c == WALL)
        return 0;
      if (c == ME)
        ++me;
      else if (c == OPP)
        ++opp;
    }
    if (me && opp)
      return 0;
    if (me)
      return m_W[me];
    if (opp)
      return -m_W[opp];
    return 0;
  }

  void MyPlayer::bump_near(int x, int y, int delta)
  {
    for (int dx = -2; dx <= 2; ++dx)
      for (int dy = -2; dy <= 2; ++dy)
      {
        int nx = x + dx, ny = y + dy;
        if (nx >= 0 && ny >= 0 && nx < m_nx && ny < m_ny)
          m_near[nx * m_ny + ny] += delta;
      }
  }

  // Поставить фишку и пересчитать оценку только по окнам, содержащим клетку.
  void MyPlayer::apply(int x, int y, std::int8_t p)
  {
    for (int d = 0; d < 4; ++d)
      for (int k = 0; k < m_len; ++k)
        m_score -= window_value(x - DX[d] * k, y - DY[d] * k, d);
    m_b[x * m_ny + y] = p;
    for (int d = 0; d < 4; ++d)
      for (int k = 0; k < m_len; ++k)
        m_score += window_value(x - DX[d] * k, y - DY[d] * k, d);
    bump_near(x, y, +1);
  }

  void MyPlayer::undo(int x, int y, long long saved)
  {
    m_b[x * m_ny + y] = EMPTY;
    bump_near(x, y, -1);
    m_score = saved;
  }

  // Собрана ли линия длины >= win_len через клетку (x, y)?
  bool MyPlayer::wins(int x, int y, std::int8_t p) const
  {
    for (int d = 0; d < 4; ++d)
    {
      int cnt = 1;
      for (int s = 1; at(x + DX[d] * s, y + DY[d] * s) == p; ++s)
        ++cnt;
      for (int s = 1; at(x - DX[d] * s, y - DY[d] * s) == p; ++s)
        ++cnt;
      if (cnt >= m_len)
        return true;
    }
    return false;
  }

  // Быстрая оценка полезности хода в клетку (для упорядочивания ходов):
  // атака (свои линии) + защита (блокировка линий соперника).
  long long MyPlayer::cell_priority(int x, int y, std::int8_t mover) const
  {
    long long s = 0;
    for (int d = 0; d < 4; ++d)
      for (int k = 0; k < m_len; ++k)
      {
        int sx = x - DX[d] * k, sy = y - DY[d] * k;
        int mv = 0, ot = 0;
        bool bad = false;
        for (int i = 0; i < m_len; ++i)
        {
          std::int8_t c = at(sx + DX[d] * i, sy + DY[d] * i);
          if (c == WALL)
          {
            bad = true;
            break;
          }
          if (c == mover)
            ++mv;
          else if (c != EMPTY)
            ++ot;
        }
        if (bad || (mv && ot))
          continue;
        if (!mv && !ot)
          s += 1;
        else if (!ot)
          s += m_att[mv];
        else
          s += m_def[ot];
      }
    return s;
  }

  // Кандидаты: свободные клетки в радиусе 2 от занятых, лучшие beam по приоритету.
  int MyPlayer::gen(int ply, std::int8_t mover, int beam)
  {
    auto &v = m_cands[ply];
    v.clear();
    for (int x = 0; x < m_nx; ++x)
      for (int y = 0; y < m_ny; ++y)
      {
        int idx = x * m_ny + y;
        if (m_b[idx] == EMPTY && m_near[idx] > 0)
          v.push_back({x, y, cell_priority(x, y, mover)});
      }
    int n = std::min<int>(beam, (int)v.size());
    std::partial_sort(v.begin(), v.begin() + n, v.end(),
                      [](const Cand &a, const Cand &b)
                      { return a.pr > b.pr; });
    // Форсированные ходы: есть ход, собирающий линию, или надо блокировать
    // соперника, у которого до линии не хватает одной клетки.
    if (n > 0 && v[0].pr >= m_def[m_len - 1])
      n = 1;
    return n;
  }

  int MyPlayer::negamax(int depth, int ply, int alpha, int beta,
                        std::int8_t side)
  {
    if ((++m_nodes & 255) == 0 && std::chrono::steady_clock::now() >= m_deadline)
      m_timeout = true;
    if (m_timeout)
      return 0;
    int n = gen(ply, side, BEAM);
    if (n == 0)
      return 0; // нет ходов - считаем ничьей
    int best = -INF;
    for (int i = 0; i < n; ++i)
    {
      Cand c = m_cands[ply][i];
      long long saved = m_score;
      apply(c.x, c.y, side);
      int val;
      if (wins(c.x, c.y, side))
        val = WIN - ply;
      else if (depth <= 1)
        val = (int)(side == ME ? m_score : -m_score);
      else
        val = -negamax(depth - 1, ply + 1, -beta, -alpha, other(side));
      undo(c.x, c.y, saved);
      if (m_timeout)
        return 0;
      if (val > best)
        best = val;
      if (best > alpha)
        alpha = best;
      if (alpha >= beta)
        break;
    }
    return best;
  }

  // Один проход итеративного углубления; возвращает оценку лучшего хода.
  int MyPlayer::search_root(int depth, int hint_x, int hint_y, int &bx,
                            int &by)
  {
    int n = gen(0, ME, ROOT_BEAM);
    auto &v = m_cands[0];
    for (int i = 0; i < n; ++i) // ход с прошлой итерации ставим первым
      if (v[i].x == hint_x && v[i].y == hint_y)
      {
        std::rotate(v.begin(), v.begin() + i, v.begin() + i + 1);
        break;
      }
    int alpha = -INF, best = -INF;
    bx = by = -1;
    for (int i = 0; i < n; ++i)
    {
      Cand c = v[i];
      long long saved = m_score;
      apply(c.x, c.y, ME);
      int val;
      if (wins(c.x, c.y, ME))
        val = WIN;
      else
        val = -negamax(depth - 1, 1, -INF, -alpha, OPP);
      undo(c.x, c.y, saved);
      if (m_timeout)
        return 0;
      if (val > best)
      {
        best = val;
        bx = c.x;
        by = c.y;
      }
      if (best > alpha)
        alpha = best;
    }
    return best;
  }

  Point MyPlayer::make_move(const State &game)
  {
    auto t0 = std::chrono::steady_clock::now();
    load(game, game.get_current_player());
    m_deadline = t0 + std::chrono::milliseconds(m_time_limit_ms);
    m_timeout = false;
    m_nodes = 0;
    m_last_depth = 0;

    int bx = -1, by = -1;
    int n = gen(0, ME, ROOT_BEAM);
    if (n > 0)
    {
      bx = m_cands[0][0].x; // запасной ход - лучший по приоритету
      by = m_cands[0][0].y;
    }
    else
    { // на поле нет фишек: ближайшая к центру свободная клетка
      int bd = INF;
      for (int x = 0; x < m_nx; ++x)
        for (int y = 0; y < m_ny; ++y)
          if (m_b[x * m_ny + y] == EMPTY)
          {
            int d = std::abs(2 * x - (m_nx - 1)) + std::abs(2 * y - (m_ny - 1));
            if (d < bd)
            {
              bd = d;
              bx = x;
              by = y;
            }
          }
    }

    if (n > 0)
    {
      for (int depth = 2; depth <= MAX_DEPTH; depth += 2)
      {
        int rx, ry;
        int val = search_root(depth, bx, by, rx, ry);
        if (m_timeout || rx < 0)
          break;
        bx = rx;
        by = ry;
        m_last_depth = depth;
        if (std::abs(val) >= WIN - 100)
          break; // найден форсированный выигрыш/проигрыш
      }
    }

    m_last_nodes = m_nodes;
    m_last_ms = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - t0)
                    .count();
    if (bx < 0)
      return Point{0, 0};
    return Point{bx, by};
  }

}; // namespace ttt::my_player