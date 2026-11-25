#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <sqlite3.h>
#include <iterator>
#include <random>
#include <numeric>

#include "ocgapi.h"
#include "ocgapi_constants.h"

const std::string CARD_DB_PATH = "BabelCDB/cards.cdb";
const std::string SCRIPT_PATH  = "CardScripts/";
std::vector<std::string> scriptDirectories = {
    "CardScripts/official/",
    "CardScripts/pre-errata/",
};
const uint32_t STARTING_LP = 8000;
const uint32_t STARTING_DRAW_COUNT = 5;
const uint32_t DRAW_COUNT_PER_TURN = 1;

struct DuelContext
{
    sqlite3* cardDatabase = nullptr;

    std::string cardDatabasePath = CARD_DB_PATH;
    std::string scriptDirectory = SCRIPT_PATH;
};

std::string ReadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Could not open: " + path);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return buffer.str();
}

bool LoadLuaFile(OCG_Duel duel, const std::string& path) {
    std::string source = ReadFile(path);

    return OCG_LoadScript(
        duel,
        source.data(),
        static_cast<uint32_t>(source.size()),
        path.c_str()
    ) > 0;
} 

std::vector<int> initial_shuffle(std::size_t size)
{
    std::vector<int> result(size);
    std::iota(result.begin(), result.end(), 0);
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::shuffle(result.begin(), result.end(), gen);
    return result;
}

void cardReader(void* payload, uint32_t code, OCG_CardData* data)
{
    auto* context = static_cast<DuelContext*>(payload);

    if (!context || !context->cardDatabase || !data)
        return;

    sqlite3_stmt* statement = nullptr;

    const char* sql =
        "SELECT alias, setcode, type, atk, def, level, "
        "race, attribute "
        "FROM datas WHERE id = ?";

    int rc = sqlite3_prepare_v2(
        context->cardDatabase,
        sql,
        -1,
        &statement,
        nullptr
    );

    if (rc != SQLITE_OK)
    {
        std::cerr << "[CARD] SQL preparation failed: "
                << sqlite3_errmsg(context->cardDatabase)
                << " (code " << rc << ")"
                << std::endl;

        return;
    }

    rc = sqlite3_bind_int64(statement, 1, code);

    if (rc != SQLITE_OK)
    {
        std::cerr << "[CARD] SQL binding failed: "
                << sqlite3_errmsg(context->cardDatabase)
                << std::endl;

        sqlite3_finalize(statement);
        return;
    }

    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        data->code = code;
        data->alias = static_cast<uint32_t>(
            sqlite3_column_int64(statement, 0));

        // Decode packed setcodes.
        uint64_t packedSetcode =
            static_cast<uint64_t>(
                sqlite3_column_int64(statement, 1));

        std::vector<uint16_t> setcodes;

        for (int i = 0; i < 4; ++i)
        {
            uint16_t value =
                static_cast<uint16_t>(packedSetcode & 0xFFFF);

            if (value == 0)
                break;

            setcodes.push_back(value);
            packedSetcode >>= 16;
        }

        // Allocate a zero-terminated setcode array.
        data->setcodes = new uint16_t[setcodes.size() + 1];

        for (size_t i = 0; i < setcodes.size(); ++i)
            data->setcodes[i] = setcodes[i];

        data->setcodes[setcodes.size()] = 0;

        data->type = static_cast<uint32_t>(
            sqlite3_column_int64(statement, 2));

        data->attack = sqlite3_column_int(statement, 3);
        data->defense = sqlite3_column_int(statement, 4);

        data->level = static_cast<uint32_t>(
            sqlite3_column_int64(statement, 5));

        data->race = static_cast<uint64_t>(
            sqlite3_column_int64(statement, 6));

        data->attribute = static_cast<uint32_t>(
            sqlite3_column_int64(statement, 7));

    }
    else
    {
        std::cerr << "[CARD] Card not found: "
                  << code << std::endl;
    }

    sqlite3_finalize(statement);
}

void cardReaderDone(void* payload, OCG_CardData* data)
{
    auto* context = static_cast<DuelContext*>(payload);

    if (!context || !data)
        return;

    delete[] data->setcodes;
    data->setcodes = nullptr;
}


int scriptReader(void* payload, OCG_Duel duel, const char* name)
{
    auto* context = static_cast<DuelContext*>(payload);

    if (!context || !name)
        return 0;

    std::string path = context->scriptDirectory + name;

    std::ifstream file(path, std::ios::binary);

    if (!file.is_open())
    {
        bool foundFile = false;

        for (const auto& directory : scriptDirectories)
        {
            path = directory + name;
            file.open(path, std::ios::binary);

            if (file.is_open())
            {
                foundFile = true;
                break;
            }
        }

        if (!foundFile)
        {
            return 0;
        }
    }

    std::string script(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    if (script.empty())
    {
        std::cerr << "[SCRIPT] Empty script: "
                  << path << std::endl;
        return 0;
    }

    return OCG_LoadScript(
        duel,
        script.data(),
        static_cast<uint32_t>(script.size()),
        name
    );
}


void logHandler(void* payload, const char* message, int type)
{
    auto* context = static_cast<DuelContext*>(payload);

    if (!context || !message)
        return;

    switch (type)
    {
        case OCG_LOG_TYPE_ERROR:
            std::cerr << "[OCG ERROR] ";
            break;

        case OCG_LOG_TYPE_FROM_SCRIPT:
            std::cout << "[OCG SCRIPT] ";
            break;

        case OCG_LOG_TYPE_FOR_DEBUG:
            std::cout << "[OCG DEBUG] ";
            break;

        default:
            std::cout << "[OCG] ";
            break;
    }

    std::cout << message << std::endl;
}

std::string get_version(){
    int major = 0;
    int minor = 0;

    OCG_GetVersion(&major, &minor);

    return std::to_string(major) + "." + std::to_string(minor);
}

OCG_Duel create_duel(DuelContext& context)
{
    if (sqlite3_open(
            context.cardDatabasePath.c_str(),
            &context.cardDatabase) != SQLITE_OK)
    {
        std::cerr << "Failed to open card database: "
                  << sqlite3_errmsg(context.cardDatabase)
                  << std::endl;

        if (context.cardDatabase)
        {
            sqlite3_close(context.cardDatabase);
            context.cardDatabase = nullptr;
        }

        return nullptr;
    }

    OCG_Duel duel = nullptr;

    OCG_DuelOptions options{};

    OCG_Player Player_one{
        STARTING_LP,
        STARTING_DRAW_COUNT,
        DRAW_COUNT_PER_TURN
    };

    OCG_Player Player_two{
        STARTING_LP,
        STARTING_DRAW_COUNT,
        DRAW_COUNT_PER_TURN
    };

    options.seed[0] = 12345;
    options.seed[1] = 67890;
    options.seed[2] = 11111;
    options.seed[3] = 22222;

    options.team1 = Player_one;
    options.team2 = Player_two;

    options.cardReader = cardReader;
    options.payload1 = &context;

    options.scriptReader = scriptReader;
    options.payload2 = &context;

    options.logHandler = logHandler;
    options.payload3 = &context;

    options.cardReaderDone = cardReaderDone;
    options.payload4 = &context;

    int success = OCG_CreateDuel(&duel, &options);

    if (success != OCG_DUEL_CREATION_SUCCESS)
    {
        std::cerr << "OCG_CreateDuel failed! Error code: "
                  << success << std::endl;

        sqlite3_close(context.cardDatabase);
        context.cardDatabase = nullptr;

        return nullptr;
    }

    if(!LoadLuaFile(duel, "CardScripts/constant.lua")){
        std::cerr << "[CREATE DUEL] Can't find constant.lua" << std::endl;
    }

    if(!LoadLuaFile(duel, "CardScripts/utility.lua")){
        std::cerr << "[CREATE DUEL] Can't find utility.lua" << std::endl;
    }
    //LoadLuaFile(duel, "CardScripts/procedure.lua");

    return duel;
}

void load_deck(std::vector<std::vector<uint32_t>> deck, uint32_t player, OCG_Duel duel){
    std::vector<int> main_order = initial_shuffle(deck[0].size());
    std::vector<int> extra_order = initial_shuffle(deck[1].size());
    for(uint32_t i = 0; i < deck[0].size(); ++i){
        std::cout << main_order[i] << std::endl;
        OCG_NewCardInfo card{};

        card.team = player;
        card.duelist = 0;
        card.code = deck[0][main_order[i]];
        card.con = player;
        card.loc = LOCATION_DECK;
        card.seq = i;
        card.pos = POS_FACEDOWN_DEFENSE;

        OCG_DuelNewCard(duel, &card);
    }

    for(uint32_t i = 0; i < deck[1].size(); ++i){
        OCG_NewCardInfo card{};

        card.team = player;
        card.duelist = 0;
        card.code = deck[1][extra_order[i]];
        card.con = player;
        card.loc = LOCATION_EXTRA;
        card.seq = i;
        card.pos = POS_FACEDOWN_DEFENSE;

        OCG_DuelNewCard(duel, &card);
    }
}

void start_duel(OCG_Duel duel){
    OCG_StartDuel(duel);
}

int process_duel(OCG_Duel duel){
    return OCG_DuelProcess(duel);
}

std::vector<uint8_t> get_message(OCG_Duel duel){
    uint32_t length = 0;

    auto* message = static_cast<const uint8_t*>(
        OCG_DuelGetMessage(duel, &length)
    );

    if (!message || length == 0)
        return {};

    return std::vector<uint8_t>(message, message + length);
}

void destroy_duel(OCG_Duel duel, DuelContext& context)
{
    if (duel)
        OCG_DestroyDuel(duel);

    if (context.cardDatabase)
    {
        sqlite3_close(context.cardDatabase);
        context.cardDatabase = nullptr;
    }
}

void diagnostic_test(OCG_Duel duel){
    OCG_StartDuel(duel);

    for (int i = 0; i < 5; ++i)
    {
        int status = OCG_DuelProcess(duel);

        uint32_t length = 0;
        auto* message = static_cast<const uint8_t*>(
            OCG_DuelGetMessage(duel, &length)
        );

        std::cout << "\n--- Process iteration " << i << " ---\n";
        std::cout << "Status: " << status << '\n';
        std::cout << "Message length: " << length << '\n';

        if (message && length > 0)
        {
            for (uint32_t j = 0; j < length; ++j)
            {
                printf("%02X ", message[j]);
            }

            printf("\n");
        }

        if (status == OCG_DUEL_STATUS_END ||
            status == OCG_DUEL_STATUS_AWAITING)
        {
            break;
        }
    }
}