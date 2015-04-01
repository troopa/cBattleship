#include "GameServer.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread.hpp> 
#include <algorithm>

GameServer::GameServer()
    : state_machine(CHECK_FOR_CONNECTIONS, *this), server() {
    std::cout << "BattleShipServer listening on 0.0.0.0:13477 ..." << std::endl;
}

GameServer::~GameServer() {
}

GameServer::StateMachineType::StateMap GameServer::get_state_map() {
    StateMachineType::StateMap map;
    map[CHECK_FOR_CONNECTIONS] =  &GameServer::check_for_connections;
    map[SETUP_GAME] = &GameServer::setup_game;
    map[TURN_WAIT] = &GameServer::turn_wait;
    return map;
}

PlayerNetworkPackage GameServer::get_input() {
    while(input_queue.empty()) {
        auto & connections = server.get_connections();
        for(auto it = players.begin(); it != players.end(); it++) {
            conn_id_t id = it->first;
            bool still_connected = std::any_of(connections.begin(), connections.end(), [id](std::unique_ptr<Connection> & connection) {
                return connection->get_id() == id;
            });
            if(!still_connected) {
                players.erase(id);
            }
        }
        for(auto it = connections.begin(); it != connections.end(); it++) {
            auto& connection = **it;
            handle_connection(connection);
        }
    }

    std::lock_guard<std::mutex> lock(queue_lock);
    PlayerNetworkPackage player_command = input_queue.front();
    input_queue.pop();
    return player_command;
}

void GameServer::handle_connection(Connection & connection) {
    if(!is_new_connection(connection)) return;

    if(can_handle_new_connection()) {
        register_new_connection(connection);
    } else {
        connection.disconnect();
    }
}

void GameServer::handle_player_connection(Connection & connection) {
    auto & player = *players[connection.get_id()].get();
    connection.read([this, &player, &connection](NetworkPackage& command) {
        std::cout << "received command #" << (int)command.get_package_nr() << " from client" << std::endl;
        if(is_authenticated(command, player)) {
            std::lock_guard<std::mutex> lock(queue_lock);
            PlayerNetworkPackage pcmd(command, player);
            input_queue.push(pcmd);
        } else {
            std::cout << "command not properly authenticated" << std::endl;
        }
        handle_player_connection(connection);
    });
}

bool GameServer::is_authenticated(NetworkPackage & command, Player & player) {
    AuthenticatedNetworkPackage* authenticated_package = dynamic_cast<AuthenticatedNetworkPackage*>(&command);
    if(authenticated_package == nullptr) return true;
    return authenticated_package->get_identity() == player.get_identity();
}

bool GameServer::is_new_connection(Connection & connection) {
    return players.find(connection.get_id()) == players.end();
}

bool GameServer::can_handle_new_connection() {
    return players.size() < 2;
}

void GameServer::register_new_connection(Connection & connection) {
    players[connection.get_id()] = std::unique_ptr<Player>(new Player(connection));
    handle_player_connection(connection);
}

void GameServer::run() {
    while(!state_machine.has_terminated()) {
        auto input = get_input();
        state_machine.run_state(input);
    }
}

void GameServer::next_player() {
    if(current_player != players_playing.end()) {
        current_player++;
    }
    if(current_player == players_playing.end()) {
        current_player = players_playing.begin();
    }
}

Player& GameServer::get_enemy() {
    auto current = current_player;
    next_player();
    auto enemy = current_player;
    current_player = current;
    return **enemy;
}

void GameServer::request_turn(bool enemy_hit, position_t position) {
    next_player();
    std::cout << "requesting turn from" << (*current_player)->get_name() << std::endl;
    TurnRequestPackage turn_request_package;
    turn_request_package.set_enemy_hit(enemy_hit);
    turn_request_package.set_position(position);
    (*current_player)->get_connection().write(turn_request_package);
}


GameServerState GameServer::check_for_connections(PlayerNetworkPackage player_package) {
    std::cout << __FUNCTION__ << std::endl;
    Player& player = player_package.get_player();
    NetworkPackage& package = player_package.get_package();

    if(is_package_of_type<PlayerJoinPackage>(package)) {
        std::cout << "PlayerJoinPackage received" << std::endl;
        PlayerJoinPackage & p = cast_package<PlayerJoinPackage>(package);
        player.set_name(p.get_player_name());
        PlayerJoinAnswerPackage answer;

        auto randchar = []() -> char {
            const char charset[] = "abcdefghijklmnopqrstuvwxyz";
            const size_t size = sizeof(charset) - 1;
            return charset[rand() % size];
        };
        std::string identity(IDENTITY_LENGTH, 0);
        std::generate_n(identity.begin(), IDENTITY_LENGTH, randchar);
        answer.set_identity(identity);
        player.set_identity(identity);
        player.get_connection().write(answer);
        players_playing.push_back(&player);
    }

    if(players_playing.size() == 2) {
        std::for_each(players_playing.begin(), players_playing.end(), [](Player* player) {
            GameReadyPackage package;
            player->get_connection().write(package);
        });
        return SETUP_GAME;
    } else {
        return CHECK_FOR_CONNECTIONS;
    }
}

GameServerState GameServer::setup_game(PlayerNetworkPackage player_package) {
    std::cout << __FUNCTION__ << std::endl;
    Player& player = player_package.get_player();
    NetworkPackage& package = player_package.get_package();

    if(is_package_of_type<ShipPlacementPackage>(package)) {
        player.set_ready_to_start(true);
    }

    bool players_are_ready_to_start = std::all_of(players_playing.begin(), players_playing.end(), [](Player *player) {
        return player->is_ready_to_start();
    });
    
    if(players_are_ready_to_start) {
        auto last_player = players_playing.end();
        last_player--;
        current_player = last_player;
        request_turn(false, position());
        return TURN_WAIT;
    }
    return SETUP_GAME;
}

GameServerState GameServer::turn_wait(PlayerNetworkPackage player_package) {
    std::cout << __FUNCTION__ << std::endl;
    Player& player = player_package.get_player();
    NetworkPackage& package = player_package.get_package();

    if(is_package_of_type<TurnPackage>(package)) {
        if(&player == *current_player) {
            TurnPackage & p = cast_package<TurnPackage>(package);
            position_t position = p.get_position();
            get_enemy().get_battle_field().hit_field(position);
            request_turn(true, position);
        }
    }
    return TURN_WAIT;
}
