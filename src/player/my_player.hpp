#pragma once

#include "core/game.hpp"

#include <chrono>
#include <cstdint>
#include <vector>

namespace ttt::my_player
{

  using game::Event;
  using game::IPlayer;
  using game::Point;
  using game::Sign;
  using game::State;

  // Бот: минимакс (негамакс) с альфа-бета отсечением, итеративным углублением,
  // ограничением по времени и упорядочиванием ходов.
  class MyPlayer : public IPlayer
  {
  public:
    MyPlayer(const char *name) : m_sign(Sign::NONE), m_name(name) {}
    void set_sign(Sign sign) override;
    Point make_move(const State &game) override;
    const char *get_name() const override;

    // Настройка и статистика (для тестов и отчета).
    void set_time_limit_ms(int ms) { m_time_limit_ms = ms; }
    int last_depth() const { return m_last_depth; }
    long long last_nodes() const { return m_last_nodes; }
    double last_time_ms() const { return m_last_ms; }

  private:
    struct Cand
    {
      int x;
      int y;
      long long pr;
    };

    Sign m_sign;
    const char *m_name;

    // Внутренняя доска: 0 - пусто, 1 - я, 2 - соперник, 3 - препятствие.
    int m_nx = 0, m_ny = 0, m_len = 5;
    std::vector<std::int8_t> m_b;
    std::vector<int> m_near; // сколько занятых клеток в радиусе 2
    long long m_score = 0;   // оценка позиции с точки зрения "меня"
    long long m_W[16], m_att[16], m_def[16];
    std::vector<std::vector<Cand>> m_cands;

    // Управление поиском.
    int m_time_limit_ms = 70;
    std::chrono::steady_clock::time_point m_deadline;
    bool m_timeout = false;
    long long m_nodes = 0;
    int m_last_depth = 0;
    long long m_last_nodes = 0;
    double m_last_ms = 0;

    void load(const State &game, Sign me);
    std::int8_t at(int x, int y) const;
    long long window_value(int sx, int sy, int d) const;
    void apply(int x, int y, std::int8_t p);
    void undo(int x, int y, long long saved);
    void bump_near(int x, int y, int delta);
    bool wins(int x, int y, std::int8_t p) const;
    long long cell_priority(int x, int y, std::int8_t mover) const;
    int gen(int ply, std::int8_t mover, int beam);
    int negamax(int depth, int ply, int alpha, int beta, std::int8_t side);
    int search_root(int depth, int hint_x, int hint_y, int &bx, int &by);
  };

}; // namespace ttt::my_player