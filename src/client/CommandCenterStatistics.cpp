#include "CommandCenterStatistics.h"

CommandCenterStatistics::CommandCenterStatistics(int height, int width, int x, int y) : CursesWindow(x,y,height,width,stdscr)
{
    this->set_attributes(COLOR_PAIR(GREEN));
    this->create_box(1,0);
    this->shots = 10;
    this->hits = 3;
}

CommandCenterStatistics::~CommandCenterStatistics(){

}

float CommandCenterStatistics::calculate_hit_rate(){
    if(this->shots != 0)
        return this->hits / this->shots * 100;
}

void CommandCenterStatistics::increase_shots(bool is_hit){
    ++this->shots;
    if(is_hit)
        ++this->hits;
}

void CommandCenterStatistics::print_ships(BattleField &battle_field){
    std::map<unsigned int, unsigned int> ship_list = battle_field.get_ships_available();
    std::string ship_names[] = {"Cruiser","Destroyer","Battleship","Aircraft Carrier"};
    int y = 1;
    for(auto iterator = ship_list.begin(); iterator != ship_list.end(); ++iterator){
        wattron(window,COLOR_PAIR(RED));
        mvwprintw(window,y++,3, "%ix", iterator->second);
        wattron(window,COLOR_PAIR(GREEN));
        wprintw(window," %s (%i-ship)", ship_names[iterator->first - 2].c_str(),iterator->first);
    }

    mvwprintw(window,y+=2,3,"hit ratio: %.2f %%", calculate_hit_rate());
    wrefresh(window);
}

void CommandCenterStatistics::print_keys(){
    int y = getcury(window);
    mvwprintw(window,y++,3,"i - insert your fleet");
    
    wrefresh(window);
}
