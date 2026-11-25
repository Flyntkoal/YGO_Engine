#include "Interface/Brain.cpp"
#include "deck_library.h"
#include "card_library.h"

void parse_message(std::vector<uint8_t> message);
void draw_card(std::vector<uint8_t> message);

int main() {
    DuelContext context;
    
    OCG_Duel cur_duel = create_duel(context);
    if(!cur_duel){
        return 1;
    }
    load_deck(YUGI_KAIBA_BEAT, 0, cur_duel);
    load_deck(YUGI_KAIBA_BEAT, 1, cur_duel);
    //diagnostic_test(cur_duel);
    start_duel(cur_duel);
    process_duel(cur_duel);
    std::vector<uint8_t> message = get_message(cur_duel);
    parse_message(message);
    destroy_duel(cur_duel, context);

    return 0;
}

void parse_message(std::vector<uint8_t> message){
    if (message.empty())
        return;

    switch(static_cast<int>(message[4])){
        case 90:
            draw_card(message);
    }

    /*for (size_t i = 0; i < message.size(); ++i)
    {
        uint8_t current_byte = message[i];

        // TODO: Determine what this byte represents
        std::cout << "Byte " << i << ": "
                  << static_cast<int>(current_byte) << '\n';
    }*/
}

void draw_card(std::vector<uint8_t> message){
    int num_cards = static_cast<int>(message[6]);
    std::cout << "Player ";
    if(static_cast<int>(message[5]) == 0){
        std::cout << "1 ";
    }
    else{
        std::cout << "2 ";
    }
    std::cout << "drew " << num_cards << " cards:" << std::endl;
    for(size_t i = 7; i < message.size(); ++i){
        uint8_t current_byte = message[i];
        std::cout << "Byte " << i << ": "
                  << static_cast<int>(current_byte) << '\n';
    }
    std::cout << std::endl;
}