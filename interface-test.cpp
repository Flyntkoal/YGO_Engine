#include "Interface/Brain.cpp"
#include "deck_library.h"

int main() {
    std::cout << get_version() << std::endl;
    DuelContext context;
    
    OCG_Duel cur_duel = create_duel(context);
    if(!cur_duel){
        return 1;
    }
    load_deck(YUGI_KAIBA_BEAT, 0, cur_duel);
    load_deck(YUGI_KAIBA_BEAT, 1, cur_duel);
    //diagnostic_test(cur_duel);
    start_duel(cur_duel);
    std::cout << process_duel(cur_duel) << std::endl;
    get_message(cur_duel);
    destroy_duel(cur_duel, context);

    return 0;
}