#include "EnhancedClient.h"
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread.hpp> 
#include <algorithm>

EnhancedClient::EnhancedClient(Connection &connection)
    : state_machine(GET_IDENTITY, *this),  connection(connection) {
}

EnhancedClient::~EnhancedClient() {
}

EnhancedClient::StateMachineType::StateMap EnhancedClient::get_state_map() {
    StateMachineType::StateMap map;
    map[GET_IDENTITY] = &EnhancedClient::get_identity;
    map[WAIT_FOR_GAME_START] = &EnhancedClient::wait_for_game_start;
    return map;
}

std::string EnhancedClient::ask_user(std::string prompt, std::string default_value) {
    std::string value;
    std::cout << prompt << std::flush;
    std::cin >> value;
    if(!std::cin.good()) value = default_value;
    return value;
}

void EnhancedClient::ask_ship_placement() {
    while(!you.get_battle_field().all_ships_placed()) {
        print_ships_available();
        auto length = ask_ship_length();
        auto orientation = ask_ship_orientation();
        auto position = ask_ship_position(length, orientation);
        you.get_battle_field().add_ship(length, orientation, position);
    }
}

unsigned int EnhancedClient::ask_ship_length() {
    unsigned int chosen_length = 0;
    auto ships_available = you.get_battle_field().get_ships_available();
    bool ok = false;
    while(!ok) {
        try {
            std::string input = ask_user("Choose Length: ", "-");
            chosen_length = boost::lexical_cast<unsigned int>(input);
            if(ships_available.find(chosen_length) != ships_available.end()) {
                if(ships_available[chosen_length] > 0) {
                    ok = true;
                } else {
                    std::cout << "no more " << get_ship_name_by_length(chosen_length) << "s available" << std::endl;
                }
            } else {
                std::cout << "there are no ships of length " << chosen_length << std::endl;
            }
        } catch(boost::bad_lexical_cast&) {
            std::cout << "invalid input" << std::endl;
        }
    }
    return chosen_length;
}

orientation_t EnhancedClient::ask_ship_orientation() {
    orientation_t orientation = ORIENTATION_HORIZONTAL;
    bool ok = false;
    while(!ok) {
        std::string input = ask_user("Choose Orientation ([v]ertical/[h]orizontal): ", "");
        if(input == "v" || input == "h") {
            orientation = input == "v" ? ORIENTATION_VERTICAL : ORIENTATION_HORIZONTAL;
            ok = true;
        } else {
            std::cout << "invalid orienation. type either 'v' or 'h'." << std::endl;
        }
    }
    return orientation;
}

position_t EnhancedClient::ask_ship_position(unsigned int length, orientation_t orientation) {
    position_t position;
    bool ok = false;
    while(!ok) {
        position.y = ask_ship_coord("y");
        position.x = ask_ship_coord("x");
        position_t end_position = position;
        end_position[orientation] += length;
        if(position.y <= BATTLEFIELD_HEIGHT && position.x <= BATTLEFIELD_WIDTH) {
            ok = true;
        } else {
            std::cout << "cant place ship there. out of bounds." << std::endl;
        }
    }
    return position;
}

position_coordinate_t EnhancedClient::ask_ship_coord(std::string coord_name) {
    position_coordinate_t coord = 0;
    bool ok = false;
    while(!ok) {
        try {
            std::string input = ask_user("Choose Position (" + coord_name + "): ", "");
            coord = boost::lexical_cast<position_coordinate_t>(input);
            ok = true;
        } catch(boost::bad_lexical_cast&) {
            std::cout << "invalid input" << std::endl;
        }
    }
    return coord;
}

void EnhancedClient::print_ships_available() {
    auto ships_available = you.get_battle_field().get_ships_available();
    std::cout  << " name             | length | available " << std::endl;
    std::cout  << "------------------+--------+-----------" << std::endl;
    std::for_each(ships_available.begin(), ships_available.end(), [this](std::pair<unsigned int, int> pair) {
        const unsigned int length = pair.first;
        const int number_available = pair.second;
        std::cout << " "   << std::setw(16) << get_ship_name_by_length(length)
                  << " | " << std::setw(6) << length
                  << " | " << std::setw(8) << number_available
                  << std::endl;
        
    });
}

std::string EnhancedClient::get_ship_name_by_length(unsigned int length) {
    std::string ship_names[] = {"Destroyer", "Cruiser", "Battleship", "Aircraft Carrier"};
    if(length >= 2 && length <= 5) {
        return ship_names[length - 2];
    }
    return std::string("(invalid ship)");
}

void EnhancedClient::run() {
    std::string name = ask_user("your nickname: ", "unnamed");
    PlayerJoinPackage player_join_package;
    player_join_package.set_player_name(name);
    connection.write(player_join_package);

    static std::function<void(void)> get_input;
    get_input = [this]() -> void {
        connection.read([this](NetworkPackage& command) {
            ServerNetworkPackage package(command);
            state_machine.run_state(package);
            get_input();
        });
    };
    get_input();
}


EnhancedClientState EnhancedClient::get_identity(ServerNetworkPackage server_package) {
    std::cout << "get identity" << std::endl;
    NetworkPackage &package = server_package.get_package();
    if(is_package_of_type<PlayerJoinAnswerPackage>(package)) {
         PlayerJoinAnswerPackage & answer = cast_package<PlayerJoinAnswerPackage>(package);
         you.set_identity(answer.get_identity());
         std::cout << "identity: " << you.get_identity() << std::endl;
        return WAIT_FOR_GAME_START;
    }
    return GET_IDENTITY;
}

EnhancedClientState EnhancedClient::wait_for_game_start(ServerNetworkPackage server_package) {
    NetworkPackage &package = server_package.get_package();
    if(is_package_of_type<GameReadyPackage>(package)) {
        GameReadyPackage & game_ready_package = cast_package<GameReadyPackage>(package);
        ask_ship_placement();
        ShipPlacementPackage ship_placement_package;
        ship_placement_package.set_identity(you.get_identity());
        connection.write(ship_placement_package);
    }
    return STOP;
}
