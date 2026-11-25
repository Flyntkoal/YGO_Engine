#include "Interface/Brain.cpp"
#include "deck_library.h"

void parse_message(std::vector<uint8_t> message);
void draw_card(std::vector<uint8_t>& message);
uint32_t read_uint32(const std::vector<uint8_t>& message, size_t offset);

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
}

void draw_card(
    std::vector<uint8_t>& message)
{
    // Raw MSG_DRAW layout:
    // [0]     Message type
    // [1]     Player
    // [2-5]   Number of cards drawn
    // [6-9]   First card ID
    // [10-13] First card position
    // [14-17] Second card ID
    // [18-21] Second card position
    // ...

    if (message.size() < 6)
        return;

    int player = message[1];

    uint32_t num_cards = read_uint32(message, 2);

    // Each card occupies 8 bytes.
    if (num_cards > (message.size() - 6) / 8)
    {
        std::cerr << "Invalid draw message!" << std::endl;
        return;
    }

    std::cout << "Player " << player + 1
              << " drew " << num_cards << " cards: [";

    for (uint32_t i = 0; i < num_cards; ++i)
    {
        size_t offset = 6 + (i * 8);

        // Extract card ID.
        uint32_t card_id = read_uint32(message, offset);

        // Retrieve card information.
        Card card = get_card_info(card_id);

        if (i > 0)
            std::cout << ", ";

        if (!card.name.empty())
            std::cout << card.name;
        else
            std::cout << "Unknown Card (" << card_id << ")";
    }

    std::cout << "]" << std::endl;
}

uint32_t read_uint32(
    const std::vector<uint8_t>& message,
    size_t offset)
{
    return static_cast<uint32_t>(message[offset])
         | (static_cast<uint32_t>(message[offset + 1]) << 8)
         | (static_cast<uint32_t>(message[offset + 2]) << 16)
         | (static_cast<uint32_t>(message[offset + 3]) << 24);
}