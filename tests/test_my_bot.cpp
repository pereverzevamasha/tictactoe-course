// Автоматические тесты для MyPlayer (CTest).
#include "core/game.hpp"
#include "player/my_player.hpp"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>

using namespace ttt::game;
using ttt::my_player::MyPlayer;

namespace
{

    State::Opts make_opts()
    {
        State::Opts o;
        o.rows = o.cols = 20;
        o.win_len = 5;
        o.max_moves = 0;
        return o;
    }

    // Тест 1: бот всегда возвращает координаты в пределах поля и в свободную
    // клетку — на пустом поле и на поле с уже сделанными ходами.
    bool test_move_is_legal()
    {
        auto opts = make_opts();
        RandomObstaclesFI fi(0.75, 50, 1);
        State state(opts, &fi);
        MyPlayer bot("bot");
        bot.set_sign(Sign::X);

        for (int i = 0; i < 5; ++i)
        {
            Point p = bot.make_move(state);
            if (p.x < 0 || p.x >= opts.rows || p.y < 0 || p.y >= opts.cols)
            {
                std::printf("test_move_is_legal: координаты вне поля (%d, %d)\n", p.x,
                            p.y);
                return false;
            }
            if (state.get_value(p.x, p.y) != Sign::NONE)
            {
                std::printf("test_move_is_legal: клетка (%d, %d) занята\n", p.x, p.y);
                return false;
            }
            MoveResult r = state.process_move(Sign::X, p.x, p.y);
            if (r != MoveResult::OK && r != MoveResult::WIN && r != MoveResult::DRAW)
            {
                std::printf("test_move_is_legal: неожиданный результат хода\n");
                return false;
            }
            if (r != MoveResult::OK)
                break;
            bool moved = false;
            for (int x = 0; x < opts.rows && !moved; ++x)
                for (int y = 0; y < opts.cols && !moved; ++y)
                    if (state.get_value(x, y) == Sign::NONE)
                    {
                        state.process_move(Sign::O, x, y);
                        moved = true;
                    }
        }
        return true;
    }

    // Тест 2: полная партия бота против самого себя не заканчивается
    // дисквалификацией ни одной из сторон.
    bool test_self_play_no_dq()
    {
        auto opts = make_opts();
        RandomObstaclesFI fi(0.75, 50, 1);
        State state(opts, &fi);

        MyPlayer px("botX");
        MyPlayer po("botO");
        px.set_sign(Sign::X);
        po.set_sign(Sign::O);
        px.set_time_limit_ms(30);
        po.set_time_limit_ms(30);

        Game game(opts, &fi);
        game.add_player(Sign::X, &px);
        game.add_player(Sign::O, &po);

        MoveResult r;
        int guard = 0;
        do
        {
            r = game.process();
            ++guard;
        } while (r == MoveResult::OK && guard < opts.rows * opts.cols + 5);

        if (is_dq(r))
        {
            std::printf("test_self_play_no_dq: дисквалификация, result=%d\n",
                        (int)r);
            return false;
        }
        return true;
    }

    // Тест 3: время одного хода укладывается в установленный лимит с запасом.
    bool test_move_time_limit()
    {
        auto opts = make_opts();
        RandomObstaclesFI fi(0.75, 50, 1);
        State state(opts, &fi);
        MyPlayer bot("bot");
        bot.set_sign(Sign::X);
        bot.set_time_limit_ms(70);

        auto t0 = std::chrono::steady_clock::now();
        bot.make_move(state);
        double ms = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - t0)
                        .count();

        if (ms > 100.0)
        {
            std::printf("test_move_time_limit: ход занял %.1f мс (> 100 мс)\n", ms);
            return false;
        }
        return true;
    }

} // namespace

int main()
{
    struct
    {
        const char *name;
        bool (*fn)();
    } tests[] = {
        {"test_move_is_legal", test_move_is_legal},
        {"test_self_play_no_dq", test_self_play_no_dq},
        {"test_move_time_limit", test_move_time_limit},
    };
    bool all_ok = true;
    for (auto &t : tests)
    {
        bool ok = t.fn();
        std::printf("%s: %s\n", t.name, ok ? "OK" : "FAIL");
        all_ok = all_ok && ok;
    }
    return all_ok ? 0 : 1;
}