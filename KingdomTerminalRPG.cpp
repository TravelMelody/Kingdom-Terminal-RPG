
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

// Konstan
const int BATTLE_INTERVAL = 10;
const int BAR_WIDTH       = 20;
const string LINE(51, '='); // Garis "=="

// =================
// Ubah angka angka dibawah ni buat balancing
// =================

// Bobot stat -> power
const int WEIGHT_GOLD       = 1;
const int WEIGHT_POPULATION = 5;

const int MAX_HAPPINESS = 100;

// Power musuh, Variansi, sedikit nerf musuh kalo player kalah
const double ENEMY_CURVE_BASE     = 450.0;
const double ENEMY_CURVE_LINEAR   = 38.0;
const double ENEMY_CURVE_QUAD     = 0.5;
const double ENEMY_RUBBER_BAND    = 0.4;
const double ENEMY_POWER_VARIANCE = 0.10;
const double CATCH_UP_EASE        = 0.15;

// Jumlah loot yang didapat kalo menang (power musuh * LOOT_POWER_RATE)
const int LOOT_HAPPINESS     = 25;
const double LOOT_POWER_RATE = 0.15;

// Kerugian kalo kalah
const double LOSS_GOLD       = 0.20;
const double LOSS_POPULATION = 0.20;
const double LOSS_HAPPINESS  = 0.10;

// Retreat cost
const double RETREAT_LOSS = 0.10;

// Skip battle cost
const double TRIBUTE_RATE = 0.04;

// Damage system, pake dadu ke table top
const int DICE_SKILL1           = 5;
const int DICE_SKILL2           = 10;
const int HIT_MIN               = 4; // Dadu >= angka ini = kena
const double DIE_DAMAGE_RATE    = 0.08; // Dmg per dadu
const double CRIT_DAMAGE_MULT   = 1.5;
const double CRIT_PER_HAPPINESS = 0.5;

// Stats tambahan
const int BASE_DEF           = 20;
const int BASE_PEN           = 20;
const double ENEMY_CRIT_RATE = 0.25;
const double MAX_CRIT_RATE   = 0.80;
const int MAX_PEN            = 80; 

// Growth stats musuh
const double ENEMY_CRIT_BASE      = 0.25;
const double ENEMY_CRIT_PER_CYCLE = 0.0015;
const double ENEMY_DEF_PER_CYCLE  = 0.5;
const double ENEMY_PEN_PER_CYCLE  = 0.25;
const double ENEMY_STAT_VARIANCE  = 0.15;

// Late game
const int ENEMY_LATE_START   = 80;
const double ENEMY_LATE_QUAD = 1;


// Makin rugi kalo musuh jauh lebih kuat, pengali = 1 + LOSS_GAP_SCALE * (power musuh / power player - 1), dibatasi 1 sampai LOSS_MAX_MULT
const double LOSS_GAP_SCALE = 1.0;
const double LOSS_MAX_MULT  = 3.0;
const double LOSS_MAX_RATE  = 0.75;

// Item, array[Def, Pen, Crit]
const double GEAR_BASE_VALUE[3]      = {6.0, 5.0, 4.0}; 
const double GEAR_VALUE_PER_ROUND[3] = {2.0, 1.5, 1.0};
const double GEAR_VALUE_SPREAD       = 0.5;
const double PRICE_PER_VALUE[3]      = {3.0, 3.0, 5.0};
const double PRICE_GROWTH_PER_ROUND  = 0.08;
const double GEAR_PRICE_SPREAD       = 0.25; 
const double SELL_RATE               = 0.5; // Potongan harga jual
const int SHOP_ITEMS_PER_TYPE        = 3;
const int MAX_GEAR                   = 9;

// =================

// Stats player
struct Stats {
    int gold;
    int population;
    int happiness;
};

// Outcome pilihan ubah stats
struct Outcome {
    string response;
    int gold;
    int population;
    int happiness;
};

struct Event {
    string npcName;
    string narration;
    Outcome yes;
    Outcome no;
    int prerequisite; // Syarat sebuah event muncul (-1 = none), buat 25-49
};

// Karakter player n musuh
struct Fighter {
    int power;
    int hp;
    int sp;
    int dieDmg;
    int heal;
    double critRate;
    int def;
    int pen;
};

// Loot
struct Loot {
    int gold;
    int population;
    int happiness;
};

// Item
enum GearType { GEAR_DEF = 0, GEAR_PEN = 1, GEAR_CRIT = 2 };
const string GEAR_NAME[3] = {"Def", "Pen", "Crit"};
 
struct Gear {
    int type;
    int value; // Def/Pen: +poin, Crit: +persen
    int fairPrice; // Harga wajar (tanpa acak), dasar harga jual
};

struct ShopItem {
    Gear gear;
    int price; // Harga beli (diacak)
};
 
// Musuh dateng ga diengaruhi shop.
struct Invader {
    int expectedPower;
    int power;
    int expectedDef;
    int def;
    int expectedPen;
    int pen;
    double critRate;
};

// Persen kerugian pas kalah
struct LossRates {
    double gold;
    double population;
    double happiness;
};

// Kalkulasi power player
int calcPower(const Stats &stats) {
    return stats.gold * WEIGHT_GOLD + stats.population * WEIGHT_POPULATION;
}

// Perkiraan power musuh
int calcEnemyPower(int cycle, int playerPower, double ease) {
    double curve = ENEMY_CURVE_BASE + cycle * ENEMY_CURVE_LINEAR + cycle * cycle * ENEMY_CURVE_QUAD;
    if (cycle > ENEMY_LATE_START) {
        double lateCycles = cycle - ENEMY_LATE_START;
        curve += lateCycles * lateCycles * ENEMY_LATE_QUAD;
    }
    double power = curve * (1.0 - ENEMY_RUBBER_BAND) + playerPower * ENEMY_RUBBER_BAND;
    return static_cast<int>(power * (1.0 - ease));
}

// Power musuh (perkiraan +/- acak)
int rollEnemyPower(int expectedPower) {
    double roll = (rand() / static_cast<double>(RAND_MAX)) * 2.0 - 1.0; // -1.0 sampai 1.0
    return max(1, static_cast<int>(expectedPower * (1.0 + roll * ENEMY_POWER_VARIANCE)));
}

int calcTribute(int enemyPower) {
    return max(1, static_cast<int>(enemyPower * TRIBUTE_RATE));
}
 
Loot calcLoot(int enemyPower) {
    double powerGain = enemyPower * LOOT_POWER_RATE;
    Loot loot;
    loot.gold       = max(1, static_cast<int>(powerGain / WEIGHT_GOLD));
    loot.population = max(1, static_cast<int>(powerGain / WEIGHT_POPULATION));
    loot.happiness  = LOOT_HAPPINESS;
    return loot;
}
 
void limitStats(Stats &stats) {
    if (stats.happiness > MAX_HAPPINESS) {
        stats.happiness = MAX_HAPPINESS;
    }
}

// Pengurangan stats sebesar persen
void reduceStats(Stats &stats, double goldRate, double populationRate, double happinessRate) {
    stats.gold -= static_cast<int>(stats.gold * goldRate);
    stats.population -= static_cast<int>(stats.population * populationRate);
    stats.happiness -= static_cast<int>(stats.happiness * happinessRate);
}

// Opsi loot
void applyLoot(Stats &stats, char pick, const Loot &loot) {
    switch (pick) {
        case '1': stats.gold       += loot.gold;       break;
        case '2': stats.population += loot.population; break;
        case '3': stats.happiness  += loot.happiness;  break;
    }
    limitStats(stats);
}

// Crit rate
double calcPlayerCrit(const Stats &stats) {
    return stats.happiness / 100.0 * CRIT_PER_HAPPINESS;
}

// Summon karakter berdasarkan stat
Fighter makeFighter(int power, double critRate, int def, int pen) {
    return {power, // Bobot stats
            power,
            0,
            static_cast<int>(power * DIE_DAMAGE_RATE),
            static_cast<int>(power * 0.3),
            critRate,
            def,
            pen};
}

// Def Pen calc
int applyDefense(int rawDamage, int def, int pen) {
    double effectiveDef = def * (1.0 - min(pen, 100) / 100.0);
    return static_cast<int>(rawDamage * 100.0 / (100.0 + effectiveDef));
}

// Damage stuff
int rollAttack(int diceCount, const Fighter &attacker, const Fighter &defender, const string &label) {
    int rawDamage = 0;
    cout << "\n" << label << ":";
    for (int i = 0; i < diceCount; i++) {
        int die = rand() % 6 + 1;
        cout << " " << die;
        if (die >= HIT_MIN) {
            bool crit = (rand() / static_cast<double>(RAND_MAX)) < attacker.critRate;
            if (crit) {
                rawDamage += static_cast<int>(attacker.dieDmg * CRIT_DAMAGE_MULT);
                cout << "*";
            } else {
                rawDamage += attacker.dieDmg;
            }
        }
    }
    return applyDefense(rawDamage, defender.def, attacker.pen);
}

// Rata damage buat AI and display
int expectedDamage(int diceCount, const Fighter &attacker, const Fighter &defender) {
    double hitChance = (7 - HIT_MIN) / 6.0;
    double perHit    = attacker.dieDmg * (1.0 + attacker.critRate * (CRIT_DAMAGE_MULT - 1.0));
    return applyDefense(static_cast<int>(diceCount * hitChance * perHit), defender.def, attacker.pen);
}

//  item, musuh, hukuman
double randSigned() {
    return (rand() / static_cast<double>(RAND_MAX)) * 2.0 - 1.0;
}
 
// Total bonus semua perlengkapan bertipe tertentu
int gearBonus(const vector<Gear> &gear, int type) {
    int total = 0;
    for (const Gear &g : gear) {
        if (g.type == type) {
            total += g.value;
        }
    }
    return total;
}
 
Fighter makePlayerFighter(const Stats &stats, const vector<Gear> &gear) {
    double crit = min(MAX_CRIT_RATE, calcPlayerCrit(stats) + gearBonus(gear, GEAR_CRIT) / 100.0);
    int def = BASE_DEF + gearBonus(gear, GEAR_DEF);
    int pen = min(MAX_PEN, BASE_PEN + gearBonus(gear, GEAR_PEN));
    return makeFighter(calcPower(stats), crit, def, pen);
}
 
int rollStat(int expected) {
    return max(0, static_cast<int>(expected * (1.0 + randSigned() * ENEMY_STAT_VARIANCE)));
}
 
Invader makeInvader(int cycle, int playerPower, double ease) {
    Invader inv;
    inv.expectedPower = calcEnemyPower(cycle, playerPower, ease);
    inv.power         = rollEnemyPower(inv.expectedPower);
    inv.expectedDef   = static_cast<int>(BASE_DEF + cycle * ENEMY_DEF_PER_CYCLE);
    inv.expectedPen   = min(MAX_PEN, static_cast<int>(BASE_PEN + cycle * ENEMY_PEN_PER_CYCLE));
    inv.def           = rollStat(inv.expectedDef);
    inv.pen           = min(MAX_PEN, rollStat(inv.expectedPen));
    inv.critRate      = min(MAX_CRIT_RATE, ENEMY_CRIT_BASE + cycle * ENEMY_CRIT_PER_CYCLE);
    return inv;
}
 
// Makin jauh musuh lebih kuat, makin besar kerugian
double calcGapMultiplier(double enemyToPlayerPower) {
    double mult = 1.0 + LOSS_GAP_SCALE * (enemyToPlayerPower - 1.0);
    return max(1.0, min(mult, LOSS_MAX_MULT));
}
 
LossRates calcLossRates(double enemyToPlayerPower) {
    double mult = calcGapMultiplier(enemyToPlayerPower);
    return {min(LOSS_MAX_RATE, LOSS_GOLD * mult),
            min(LOSS_MAX_RATE, LOSS_POPULATION * mult),
            min(LOSS_MAX_RATE, LOSS_HAPPINESS * mult)};
}
 
LossRates calcRetreatRates(double enemyToPlayerPower) {
    double rate = min(LOSS_MAX_RATE, RETREAT_LOSS * calcGapMultiplier(enemyToPlayerPower));
    return {rate, rate, rate};
}
 
// 6 barang: 2 Def, 2 Pen, 2 Crit. Value naik tiap round diacak, harga ikut value diacak juga
vector<ShopItem> generateShop(int cycle) {
    int round = cycle / BATTLE_INTERVAL;
    vector<ShopItem> shop;
    for (int type = 0; type < 3; type++) {
        for (int i = 0; i < SHOP_ITEMS_PER_TYPE; i++) {
            double avgValue = GEAR_BASE_VALUE[type] + GEAR_VALUE_PER_ROUND[type] * round;
            int value = max(1, static_cast<int>(avgValue * (1.0 + randSigned() * GEAR_VALUE_SPREAD) + 0.5));
            double fair = value * PRICE_PER_VALUE[type] * (1.0 + PRICE_GROWTH_PER_ROUND * round);
            int price = max(1, static_cast<int>(fair * (1.0 + randSigned() * GEAR_PRICE_SPREAD) + 0.5));
            shop.push_back({{type, value, static_cast<int>(fair + 0.5)}, price});
        }
    }
    return shop;
}
 
int calcSellPrice(const Gear &gear) {
    return max(1, static_cast<int>(gear.fairPrice * SELL_RATE));
}
 
string gearLabel(const Gear &gear) {
    return GEAR_NAME[gear.type] + " +" + to_string(gear.value) + (gear.type == GEAR_CRIT ? "%" : "");
}

// Input helper
string readLine() {
    string line;
    if (!getline(cin, line)) {
        cout << "\n";
        exit(0);
    }
    size_t start = line.find_first_not_of(" \t\r");
    size_t end   = line.find_last_not_of(" \t\r");
    return (start == string::npos) ? "" : line.substr(start, end - start + 1);
}

void waitForEnter(const string &message) {
    cout << message;
    readLine();
}

// Char valid cek, ubah kapital jadi huruf kecil
char askChar(const string &prompt, const string &valid, const string &errorMsg) {
    while (true) {
        cout << prompt;
        string line = readLine();
        if (line.size() == 1) {
            char c = static_cast<char>(tolower(static_cast<unsigned char>(line[0])));
            if (valid.find(c) != string::npos) {
                return c;
            }
        }
        cout << errorMsg << "\n";
    }
}

// Minta angka bulat antara minVal and maxVal
int askNumber(const string &prompt, int minVal, int maxVal) {
    while (true) {
        cout << prompt;
        string line = readLine();
        bool digitsOnly = !line.empty() && line.size() <= 3;
        for (char c : line) {
            if (c < '0' || c > '9') {
                digitsOnly = false;
            }
        }
        if (digitsOnly) {
            int n = atoi(line.c_str());
            if (n >= minVal && n <= maxVal) {
                return n;
            }
        }
        cout << "Input tidak valid! Pilih angka " << minVal << " - " << maxVal << "\n";
    }
}

// Opsi
char askYesNoQuit() {
    return askChar("Ya atau Tidak? (Y (yes)/N (no)): ", "ynq",
                   "Invalid input, hanya Y atau N");
}

char askConfirm() {
    return askChar("Lanjutkan? (Y (yes)/N (no)): ", "yn",
                   "Invalid input, hanya Y atau N");
}

char askPlayAgain() {
    return askChar("Ingin Bermain Lagi? (Y (yes)/N (no)): ", "yn",
                   "Invalid input, hanya Y atau N");
}
 
char askCombatAction(int sp) {
    while (true) {
        char c = askChar("Pilih aksi (1/2/3/4) : ", "1234q",
                         "Invalid input, hanya 1, 2, 3 atau 4");
        if ((c == '2' || c == '3') && sp < 1) {
            cout << "SP tidak cukup! Butuh 1 SP (SP sekarang: " << sp << ")\n";
            continue;
        }
        return c;
    }
}

// Event data
void addEvent(vector<Event> &list,
              const string &npc, const string &narration,
              const string &responseYes, const string &responseNo,
              int goldYes, int goldNo,
              int populationYes, int populationNo,
              int happinessYes, int happinessNo) {
    list.push_back({npc, narration,
                    {responseYes, goldYes, populationYes, happinessYes},
                    {responseNo,  goldNo,  populationNo,  happinessNo},
                    -1});
}

vector<Event> loadEvents() {
    vector<Event> e;

    // Events 0-24
    addEvent(e, "Pluto", "Kaisar… dunia bawah sedang butuh prajurit tambahan untuk mengamankan roh-roh yang sedang gelisah. Izinkan aku meminjam beberapa prajuritmu. Sebagai imbalan, akan ku beri kau imbalan berupa emas permata.", "Baguslah. Akan kukembalikan mereka, walau mungkin saja tidak lengkap.", "Haaa baik. Tapi aku tidak akan menjamin jika nantinya roh-roh itu tidak akan naik dan sampai kesini..", 40, -15, -5, 0, 5, -5);
    addEvent(e, "Bacchus", "Wahai kaisar! Aku punya sebuah penawaran untukmu! Buatlah sebuah pesta besar untuk rakyat! Mereka pasti akan bahagia! Kita akan berpesta hingga pagi hari.", "HAHAHAHA! Pasti kau tak akan menyesal… mungkin saja~", "Cih! Kau memang sangat membosankan", 10, -5, 15, -5, 0, -10);
    addEvent(e, "Aktor teater", "Kemuliaan bagimu, Kaisar! Aku bisa membuat pertunjukan yang sangat menghibur. Tiket pasti akan terjual habis, mau memberi modal untuk pertunjukanku?", "Pertunjukan akan segera dimulai!, Kalau aku sedang tidak malas~ hahahaha.", "Ah… sangat disayangkan. Padahal kekaisaran ini sungguh butuh asupan seni.", 5, 0, 10, -5, 5, -5);
    addEvent(e, "Sphinx", "Wahai penguasa kekaisaran aku punya sebuah teka-teki. Jika kau menjawabnya dengan benar, akan ku beri kau hadiah sekaligus berkah. Aku merasa nasibmu agak kurang baik, mau mencobanya?", "Bagus! Maka jawablah teka-tekiku dengan hati-hati.", "Membosankan… terserahlah.", 0, 0, 10, 0, 0, -5);
    addEvent(e, "Informan", "Kaisar! Aku punya informasi penting tentang kerajaan musuh! Tapi yah… aku tidak bisa menjaminnya 100% kalau itu benar. Mau mendengarnya? Ya walau tentu saja itu tidak gratis.", "Aku dengar kerajaan musuh memenangkan perang karena artefak milik kuil suci!... katanya~.", "Baiklah.. Kalau itu benar, jangan salahkan aku.", 15, 0, 5, -10, 0, -5);
    addEvent(e, "Pedagang kain", "Aku membawa kain model baru! Ini bisa saja menjadi tren di seluruh kekaisaran! … mungkin~, Mau membeli stoknya lebih awal?", "Semoga dewi Fortuna selalu di pihak kita!", "Aahhh… Kau sudah melewatkan tren!... mungkin.", 20, 0, 10, -5, 5, -5);
    addEvent(e, "Siren", "Kau tahu, aku bisa melihat masa depan dengan nyanyianku! Masa depanmu dapat terungkap… walau tidak selalu benar.", "Dengarkan… mungkin saja kali ini penglihatanku benar.", "....Kau bisa saja menyesali ini di masa depan, kau tahu?", 0, 0, 15, 0, 5, -10);
    addEvent(e, "Penjual anggur dari negeri seberang", "Wahai Kaisar! Aku datang dari negeri nan jauh dengan membawa anggur terbaik! Mau membelinya?", "Kau bisa mempercayaiku jika itu menyangkut anggur dengan kualitas terbaik… mungkin~", "Sayang sekali. Aku akan menjualnya pada penguasa lain!", 10, 0, 5, -5, 0, -5);
    addEvent(e, "Griffin", "Banyak ancaman di langit wilayahmu! Biarkan aku untuk menjaga udara di atas kotamu. Tapi… aku butuh makanan, mungkin satu atau bahkan dua ekor domba besar.", "Langitmu akan aman di bawah penjagaanku! Tak ada lagi ancaman di atas kotamu!", "...Semoga langitmu akan aman tanpa bantuanku.", 0, 5, 10, 0, -5, -10);
    addEvent(e, "Roh petarung", "Izinkan aku untuk melatih prajurit-prajurit mu. Tetapi latihan ini agak keras…", "Hebat! Akan kubuat mereka menjadi tak terkalahkan!", "Sungguh keputusan yang bodoh!", 0, 10, 5, 0, -5, 0);
    addEvent(e, "Penjaga perbatasan", "Kaisar! Ada monster di perbatasan. Izinkan kami menambah pasukan!", "Terima kasih! Kami akan berjuang sekuat tenaga!", "Baik…", -5, 15, 10, 0, -5, -5);
    addEvent(e, "Prajurit pembawa surat", "Kaisar! Seorang pria berjubah hitam menitipkan pesan yang ditunjukan untukmu. Isinya tertutup oleh segel yang belum pernah kulihat. Mau kubuka untukmu?", "Ah! Asap apa ini!", "Baik. Akan segera kusingkirkan!", 10, 0, 5, 0, 0, 0);
    addEvent(e, "Anak kecil pembawa bunga", "Kaisar! Aku memetiknya di taman. Dikatakan bunga ini akan membawa keberuntungan bagi penerimanya. Apa kau mau?", "Semoga keberuntungan selalu menyertaimu!", "O-oh baiklah…", 0, 0, 10, 0, 3, -2);
    addEvent(e, "Gladiator", "Kaisar, arena sudah sangat tua dan sangat membutuhkan renovasi! Renovasi arena dan buat rakyat bersorak!", "Kota ini akan segera dipenuhi sorakan!", "Sangat disayangkan…", 20, 0, 15, -5, 10, -5);
    addEvent(e, "Senator", "Kaisar, anggaran kota ini memerlukan dukungan materi lebih banyak. Kau tahu, agar pembangunan lebih merata dan rakyat lebih sejahtera. Setuju?", "Luar biasa! Warga kota pasti akan makmur~", "Ah… kau keras kepala sekali", 15, 0, 10, -5, 5, -5);
    addEvent(e, "Pemahat", "Wahai kaisar! Izinkan aku untuk membuat mahakarya patungmu! Akan kubuat para rakyat kekaisaran ini makin bangga padamu!", "Akan kubuat patung paling megah yang pernah dibuat manusia!", "Baik…", 10, 0, 5, 0, 0, 0);
    addEvent(e, "Senator", "Wahai kaisar, aku memiliki sebuah saran agar ekonomi semakin kuat! Naikkanlah pajak, tetapi yang ringan-ringan saja.", "Keputusan yang sangat tepat!", "Baik.", 10, 0, 0, -5, -5, 5);
    addEvent(e, "Prajurit elit", "Kaisar, izinkan aku menjadi pedang pelindungmu!", "Kau akan aman bersamaku.", "Baik", 0, 10, 10, 0, -5, -10);
    addEvent(e, "Penyair", "Kaisar! Izinkan aku membacakan puisi ciptaanku yang pasti akan mengguncang kota! Mungkin saja kebahagiaan akan datang pada rakyatmu!", "Dengarkanlah puisi ciptaanku!!!", "Ah! Seni sangat tertinggal di sini!", 0, 0, 10, 0, 5, -5);
    addEvent(e, "Prajurit", "Kaisar! Senjata kamu sudah sangat usang! Sudah sangat buruk! Izinkan kamu agar mendapat perlengkapan baru!", "Terima kasih! Kami akan menggunakannya dengan baikk!", "Baik… kami akan bertahan dengan apa yang ada.", 0, 10, 5, 0, -5, -5);
    addEvent(e, "Prajurit pembawa harta rampasan", "Kaisar! Pasukan kami menang! Kami jug amembawa harta rampasan. Mau dimasukan kas negara?", "Baik! Akan kami masukan sekarang juga!", "Akan kami bagikan kepada rakyat juga!", 30, 0, 5, 10, 5, 10);
    addEvent(e, "Wanita misterius", "Kaisar…aku adalah utusan dewa! Jika kau memberikan aku sedikit emas, maka akn kusampaikan doa perlindungan untukmu dan kekaisaran ini.", "Dewa pasti akan melindungimu… mungkin.", "Bisa-bisanya kau menolak utusan dewa!", 0, 0, 15, -10, 0, -5);
    addEvent(e, "Janus (Dewa gerbang & waktu)", "Aku bisa membuka jalan ke masa depan, tentu dengan sebuah harga! Yaitu pengetahuan, bagaimana?", "Bagus! Ini akan menguntungkanmu!", "...", 0, 0, 10, 0, 0, -5);
    addEvent(e, "Nemesis", "Aku merasakan ada kesombongan dan kekikiran yang cukup banyak di wilayah kekuasaan mu. Mau aku netralkan?", "Hahahaha. Akan kuhukum kalian wahai pengacau!!", "Cih! Hal-hal buruk akan semakin meningkat cepat atau lambat.", -5, 0, 10, 0, -5, -10);
    addEvent(e, "Ceres", "Bagaimana kalau ladangmu aku berkati?", "Ladangmu telah diberkati.", "Tidak ada peningkatan.", 20, 10, 10, 0, 10, -10);

    // Events 25-49 (follow up)
    addEvent(e, "Kurir Penjaga Perbatasan", "Kaisar! Perbatasan sudah aman! Pasukan kami berhasil mengusir monster. Rakyat sangat lega! Terima kasih atas dukungan Anda!", "Terima kasih! Kami akan terus menjaga!", "Baik…", -5, 10, 10, 0, -5, -5);
    addEvent(e, "Roh Petarung", "Kaisar! Latihanku berhasil! Prajuritmu sekarang adalah yang paling tangguh di dunia! Apakah Anda puas dengan hasilnya?", "Sempurna! Mereka siap untuk apapun!", "Sangat disayangkan…", 0, 0, 0, 0, 5, -5);
    addEvent(e, "Kepala Pekerja Arena", "Kaisar! Arena renovasi kami sudah selesai! Pertunjukan pertama akan diadakan! Rakyat sangat antusias! Apa Anda ingin pertunjukan dimulai sekarang?", "Pertunjukan dimulai! Sorakan rakyat terdengar ke seluruh kota!", "Sayang sekali… arena akan kosong dan rakyat kecewa.", 20, 0, 15, 0, 10, -10);
    addEvent(e, "Informan", "Kaisar! Informasiku ternyata benar! Musuh memiliki artefak suci itu. Sekarang aku punya intel lokasinya. Mau aku curi untuk Anda?", "Sempurna! Aku akan mengirim agen terbaikku!", "Artefak itu akan tetap milik musuh…", 30, -5, 0, 0, 0, -5);
    addEvent(e, "Pedagang Kain", "Kaisar! Kain yang Anda beli ternyata menjadi tren! Sekarang semua orang mau membelinya! Stok hampir habis! Mau Anda pesan lebih banyak lagi dengan harga lebih tinggi?", "Investasi Anda akan berlipat ganda! Emas akan mengalir deras!", "Tren akan berlalu dan stok menjadi sampah…", 25, 0, 5, -10, 0, -5);
    addEvent(e, "Siren", "Kaisar… penglihatanku sebelumnya sebagian benar! Tapi kali ini aku melihat sesuatu yang gelap di masa depan Anda… Apa Anda ingin mendengarnya?", "Dengarkan dengan seksama… (nyanyian misterius dimulai)", "Penyesalan akan menghampirimu…", -5, 0, 10, 0, 0, -5);
    addEvent(e, "Penjual Anggur", "Kaisar! Anggurku ternyata berkualitas tinggi! Tamu-tamu penting Anda sangat terkesan! Bisakah aku mengirim lebih banyak untuk acara istimewa Anda?", "Istana Anda akan selalu berbau aroma premium!", "Penguasa lain akan terus mendapat keuntungan daripadaku…", 15, 0, 10, -5, 3, -5);
    addEvent(e, "Griffin", "Kaisar! Langit wilayah Anda sekarang benar-benar aman! Tidak ada musuh udara yang berani mendekat. Aku siap selamanya! Apakah Anda puas dengan penjagaanku?", "Langit Anda akan terus aman di bawah penjagaanku!", "Sayang… pelayananku tidak dihargai.", -5, 5, 10, 0, -5, -5);
    addEvent(e, "Wanita Misterius", "Kaisar! Doa perlindunganku telah sampai kepada dewa-dewa! Mereka sangat puas dengan Anda. Kini mereka ingin memberimu berkah lebih… dengan harga tentu saja. Mau?", "Dewa-dewa akan selalu melindungimu… mungkin.", "Sayang… ini kesempatan terakhir sebelum kutukan datang.", -20, 0, 15, 0, 0, -10);
    addEvent(e, "Senator Korup", "Kaisar! Aku sudah menggunakan dana itu untuk 'proyek pembangunan' yang penting. Rakyat masih belum melihat hasilnya, tapi percayalah! Apa Anda ingin investasi lebih?", "Investasi Anda yang bijak akan membuahkan hasil… suatu hari nanti~", "Sayang… dana yang terbuang akan terus menjadi kesalahan Anda.", -30, 0, -5, 0, -5, 5);
    addEvent(e, "Nemesis", "Kaisar! Aku telah menghukum para penyombong dan pemiskin hati! Keseimbangan sudah terpulihkan. Tapi… aku merasakan ada yang masih belum seimbang di wilayah Anda. Mau aku selidiki?", "Aku akan memastikan keadilan ditegakkan!", "Dunia akan menjadi semakin tidak adil…", -5, 0, 10, 0, -5, -10);
    addEvent(e, "Ceres", "Kaisar! Panen tahun ini sangat melimpah! Ladang Anda menghasilkan dua kali lipat! Rakyat akan memiliki cukup makanan dan surplus! Apakah Anda menerima berkah ini?", "Berkahku akan terus berlanjut!", "Sayang… berkah ini akan menghilang…", 20, 10, 10, 0, 0, -10);
    addEvent(e, "Janus", "Kaisar! Pengetahuan yang kuberikan telah membuat keputusan Anda lebih bijak! Tapi sekarang pintu menunjukkan masa depan yang semakin kompleks. Ingin Anda pengetahuan lebih dalam?", "Ini akan menguntungkanmu!", "...", -10, 0, 10, 0, 5, -5);
    addEvent(e, "Kurir Aktor Teater", "Kaisar! Pertunjukan aktor itu… ternyata dia kabur dengan uang Anda! Tiket tidak pernah dijual dan rakyat sangat marah! Bagaimana Anda meresponnya?", "Rakyat akan terus mengingat kebodohan keputusan Anda itu…", "Setidaknya Anda belajar dari kesalahan…", -20, 0, -10, 0, 0, 5);
    addEvent(e, "Utusan Pluto", "Kaisar! Prajurit-prajurit Anda yang kami pinjam sangat membantu! Kami sudah selesai dan mengembalikan… setengahnya. Yang lain memutuskan tinggal di sana. Tapi kami berikan kekuatan mistis kepada yang kembali! Apakah Anda lega?", "Prajurit Anda sekarang lebih kuat dari sebelumnya!", "Sayang… setengah dari prajurit Anda tidak akan kembali…", 0, -10, 5, 0, -5, -5);
    addEvent(e, "Bacchus", "Kaisar! Pesta kami adalah yang terbaik! Rakyat sangat senang! Tapi… uang kas Anda habis dan beberapa prajurit masih mabuk. Apa Anda ingin pesta lagi?", "HAHAHAHA! Pesta akan dimulai lagi!", "Sayang… rakyat akan merasa sedih lagi…", -40, 5, 20, 0, 10, -10);
    addEvent(e, "Sphinx", "Kaisar! Anda berhasil menjawab teka-teki saya dengan benar! Berkah sudah aku curahkan. Sekarang nasib Anda lebih baik! Tapi teka-teki baru datang dari takdir… apakah Anda siap?", "Bagus! Maka jawablah dengan hati-hati…", "Sayang… takdir akan terus membingungkanmu…", -5, 0, 10, 0, 0, -5);
    addEvent(e, "Prajurit Pembawa Surat", "Kaisar! Asap apa itu!? Surat itu berisi… informasi penting tentang kekuatan tersembunyi yang bisa membantumu! Apakah Anda ingin menggunakannya?", "Informasi ini akan mengubah segalanya!", "Kesempatan emas terlewatkan…", 10, 0, 5, 0, 0, -5);
    addEvent(e, "Anak Kecil Pembawa Bunga", "Kaisar! Keberuntungan yang Anda terima dari bunga itu ternyata benar! Banyak hal baik yang terjadi! Apa saya boleh memetik bunga lagi untuk Anda?", "Semoga keberuntungan akan terus menyertaimu!", "O-oh baiklah…", -2, 0, 10, 0, 0, -3);
    addEvent(e, "Senator Pajak", "Kaisar! Pajak yang ringan itu ternyata sangat efektif! Ekonomi kekaisaran semakin kuat dan rakyat tidak keberatan! Apakah kita bisa naikkan sedikit lagi?", "Keputusan yang sangat tepat!", "Baik… tapi ekonomi akan stagnan…", 25, -5, -5, 0, 0, 5);
    addEvent(e, "Prajurit Elit", "Kaisar! Perlindunganku telah menyelamatkan nyawa Anda berkali-kali! Anda aman bersamaku. Izinkan aku terus melayani Anda?", "Anda akan selalu aman bersamaku.", "Sayang… keamanan Anda akan berkurang…", -5, 0, 10, 0, -5, -10);
    addEvent(e, "Penyair", "Kaisar! Puisiku sebelumnya sangat berhasil! Rakyat sangat tersentuh! Bolehkah saya membacakan puisi baru yang bahkan lebih mengguncang?", "Dengarkanlah puisi ciptaanku!!!", "Ah! Seni sangat tertinggal di sini…", -2, 0, 10, 0, 5, -5);
    addEvent(e, "Prajurit Perlengkapan", "Kaisar! Perlengkapan baru kami sangat berguna dalam pertempuran! Kami jauh lebih kuat sekarang! Terima kasih atas kepercayaan Anda!", "Kami akan terus menggunakannya dengan baik!", "Sayang… kami akan tetap dengan perlengkapan lama…", -15, 0, 5, 0, -3, -5);
    addEvent(e, "Prajurit Harta Rampasan", "Kaisar! Harta yang kami masukan ke kas negara telah membuat kekaisaran lebih kuat! Kami menang lagi! Ada harta rampasan baru lagi. Mau dimasukan kas negara?", "Baik! Akan kami masukan sekarang juga!", "Akan kami bagikan kepada rakyat juga!", 30, 0, 5, 10, 5, 10);
    addEvent(e, "Utusan Dewa Mars", "Kaisar! Aku adalah utusan dewa Mars! Jika Anda menyelenggarakan festival besar menghormati Mars, Anda akan mendapat berkah kemenangan dalam setiap pertempuran! Mau?", "Mars akan memberkati pertempuran Anda! KEMENANGAN ADALAH TAKDIR ANDA!", "Mengabaikan dewa perang… sebuah kesalahan yang berbahaya…", -25, 0, 10, 0, -5, -10);

    // Persyaratan follow up
    const int SEQUELS[][2] = {
        {25, 10}, {26, 9},  {27, 13}, {28, 4},  {29, 5},  {30, 6},  {31, 7}, // {follow up index, syarat index}
        {32, 8},  {33, 21}, {34, 14}, {35, 23}, {36, 24}, {37, 22}, {38, 2},
        {39, 0},  {40, 1},  {41, 3},  {42, 11}, {43, 12}, {44, 16}, {45, 17},
        {46, 18}, {47, 19}, {48, 20}
    };
    for (const auto &link : SEQUELS) {
        e[link[0]].prerequisite = link[1];
    }

    return e;
}


// Biar event ga ngulang kecuali kalo udah semua, folow up muncul kalo event original udah muncul. 
class EventDeck {
public:
    EventDeck() : events(loadEvents()), used(events.size(), false) {}

    const Event &draw() {
        vector<int> candidates = availableEvents();
        if (candidates.empty()) {
            fill(used.begin(), used.end(), false);
            candidates = availableEvents();
        }
        int pick = candidates[rand() % candidates.size()];
        used[pick] = true;
        return events[pick];
    }

private:
    vector<Event> events;
    vector<bool> used;

    vector<int> availableEvents() const {
        vector<int> result;
        for (int i = 0; i < static_cast<int>(events.size()); i++) {
            bool unlocked = events[i].prerequisite == -1 || used[events[i].prerequisite];
            if (!used[i] && unlocked) {
                result.push_back(i);
            }
        }
        return result;
    }
};

// Display
void displayMainMenu(int highScore) {
    cout << "\n" << LINE << "\n";
    cout << "                KEKAISARAN ROMAWI\n";
    cout << LINE << "\n";
    cout << "Selamat! Kau baru saja diangkat menjadi kaisar.\n";
    cout << "Kamu menjadi pemimpin tertinggi dari Kekaisaran Romawi.\n";
    cout << "Seluruh rakyat, Senat, dan para jenderal menanti\n";
    cout << "kebijaksanaan Anda.\n";
    cout << "\n";
    cout << "Akan ada banyak tantangan yang muncul dari berbagai penjuru negeri.\n";
    cout << "Sebagai Kaisar, setiap keputusan Anda akan mempengaruhi\n";
    cout << "masa depan kekaisaran.\n";
    cout << "Akankah Romawi mencapai kejayaan baru,\n";
    cout << "atau jatuh dalam kekacauan?\n";
    cout << LINE << "\n";
    cout << "Tujuan:\n";
    cout << "- Jaga ketertiban dan kemakmuran Kekaisaran\n";
    cout << "- Perkuat pasukan dan bangun kota-kota\n";
    cout << "- Lindungi rakyat dari kekacauan\n";
    cout << LINE << "\n";

    if (highScore > 0) {
        cout << "Skor Tertinggi: " << highScore << " Pengunjung\n";
        cout << LINE << "\n";
    }

    waitForEnter("Klik Enter untuk bermain...\n");
}

void displayStatus(const Stats &stats, int cycle) {
    cout << "\n" << LINE << "\n";
    cout << "                STATUS KEKAISARAN\n";
    cout << "Gold               : " << stats.gold << "\n";
    cout << "Populasi           : " << stats.population << "\n";
    cout << "Tingkat Kebahagiaan: " << stats.happiness << "%\n";
    cout << LINE << "\n";
    cout << "Pengunjung ke: #" << cycle << "\n";
}

void displayHpBar(int currentHp, int maxHp, const string &label) {
    int hp = max(0, currentHp);
    int filled = (hp * BAR_WIDTH) / maxHp;

    cout << label << " [";
    for (int i = 0; i < BAR_WIDTH; i++) {
        cout << (i < filled ? "█" : "░");
    }
    cout << "] " << hp << "/" << maxHp << "\n";
}

void displayCombatInfo(const Fighter &player, const Fighter &enemy) {
    cout << "\n" << LINE << "\n";
    cout << "Your Power    : " << player.power << "\n";
    cout << "Enemy's Power : " << enemy.power << "\n";
    cout << "Your  Crit " << static_cast<int>(player.critRate * 100) << "% | Def " << player.def << " | Pen " << player.pen << "\n";
    cout << "Enemy Crit " << static_cast<int>(enemy.critRate * 100) << "% | Def " << enemy.def << " | Pen " << enemy.pen << "\n";
    cout << LINE << "\n";
 
    displayHpBar(player.hp, player.power, "Your HP   ");
    displayHpBar(enemy.hp, enemy.power, "Enemy HP  ");
 
    cout << "Your SP       : " << player.sp << "\n";
    cout << "Skill 1 : Lempar " << DICE_SKILL1 << " dadu, rata-rata " << expectedDamage(DICE_SKILL1, player, enemy) << " dmg (0 SP)\n";
    cout << "Skill 2 : Lempar " << DICE_SKILL2 << " dadu, rata-rata " << expectedDamage(DICE_SKILL2, player, enemy) << " dmg (1 SP)\n";
    cout << "Skill 3 : Pulihkan " << player.heal << " HP (1 SP)\n";
    cout << "Aksi 4  : Mundur (mulai -" << static_cast<int>(RETREAT_LOSS * 100) << "% semua stat, makin besar kalau musuh jauh lebih kuat, tanpa rampasan)\n";
    cout << "(Dadu " << HIT_MIN << " ke atas = kena, tanda * = crit)\n";
}

// Difficulty
Stats selectDifficulty() {
    cout << "\n" << LINE << "\n";
    cout << "             PILIH TINGKAT KESULITAN\n";
    cout << LINE << "\n";
    cout << "1. Mudah   - Start: Gold 150, Populasi 150, Kebahagiaan 100 \n";
    cout << "2. Normal  - Start: Gold 100, Populasi 100, Kebahagiaan 100\n";
    cout << "3. Sulit   - Start: Gold 75, Populasi 75, Kebahagiaan 75\n";
    cout << LINE << "\n";

    char choice = askChar("Pilih (1/2/3): ", "123", "Input tidak valid! Pilih 1,2, atau 3");

    int start = 100;
    switch (choice) {
        case '1': start = 150; break;
        case '2': start = 100; break;
        case '3': start = 75;  break;
    }
    Stats stats {start, start, start};
    limitStats(stats); // Batas Happiness
    return stats;
}

// Events
void applyOutcome(Stats &stats, const Outcome &outcome) {
    stats.gold       += outcome.gold;
    stats.population += outcome.population;
    stats.happiness  += outcome.happiness;
    limitStats(stats);
}

void handleEvent(const Event &event, Stats &stats, bool &quit) {
    cout << event.npcName << " datang\n";
    cout << event.npcName << ": " << event.narration << "\n";

    while (true) {
        char choice = askYesNoQuit();

        if (choice == 'q') {
            cout << "Anda yakin untuk meninggalkan negara anda?\n";
            if (askConfirm() == 'y') {
                quit = true;
                return;
            }
            continue;
        }

        const Outcome &outcome = (choice == 'y') ? event.yes : event.no;
        applyOutcome(stats, outcome);
        cout << event.npcName << ": " << outcome.response << "\n";
        return;
    }
}

// Battle
void playerAction(char action, Fighter &player, Fighter &enemy) {
    switch (action) {
        case '1': {
            int dmg = rollAttack(DICE_SKILL1, player, enemy, "Dadu Anda");
            enemy.hp -= dmg;
            player.sp++;
            cout << "\nAnda memberikan " << dmg << " dmg!";
            break;
        }
        case '2': {
            int dmg = rollAttack(DICE_SKILL2, player, enemy, "Dadu Anda");
            enemy.hp -= dmg;
            player.sp--;
            cout << "\nAnda memberikan " << dmg << " dmg!";
            break;
        }
        case '3':
            player.hp = min(player.hp + player.heal, player.power);
            player.sp--;
            cout << "\nAnda memulihkan " << player.heal << " HP!";
            break;
    }
}

void enemyAction(Fighter &enemy, Fighter &player) {
    // Prediksi pake rata rata soalnya dadu acak
    int enemyDmg1  = expectedDamage(DICE_SKILL1, enemy, player);
    int enemyDmg2  = expectedDamage(DICE_SKILL2, enemy, player);
    int playerDmg1 = expectedDamage(DICE_SKILL1, player, enemy);
    int playerDmg2 = expectedDamage(DICE_SKILL2, player, enemy);
 
    int maxPlayerDmg = (player.sp >= 1) ? playerDmg2 : playerDmg1; // Cek dmg player
 
    bool canKillWithDmg1 = (enemyDmg1 >= player.hp);
    bool canKillWithDmg2 = (enemy.sp >= 1 && enemyDmg2 >= player.hp);
 
    bool willDieNextTurn = (enemy.hp <= maxPlayerDmg);
    bool isHPCritical    = (enemy.hp <= enemy.power * 0.35);
    bool isHPLow         = (enemy.hp <= enemy.power * 0.60);
 
    int hpAfterHeal = min(enemy.hp + enemy.heal, enemy.power); // Logika heal
    bool healIsPointless = (hpAfterHeal <= maxPlayerDmg);
 
    bool shouldHeal = (!healIsPointless) && ((enemy.sp >= 1 && (willDieNextTurn || isHPCritical)) || (enemy.sp >= 2 && isHPLow));
 
    if (canKillWithDmg1) {
        int dmg = rollAttack(DICE_SKILL1, enemy, player, "Dadu musuh");
        player.hp -= dmg;
        enemy.sp++;
        cout << "\nMusuh memberikan " << dmg << " dmg!";
    } else if (canKillWithDmg2) {
        int dmg = rollAttack(DICE_SKILL2, enemy, player, "Dadu musuh");
        player.hp -= dmg;
        enemy.sp--;
        cout << "\nMusuh memberikan " << dmg << " dmg!";
    } else if (shouldHeal) {
        enemy.hp = hpAfterHeal;
        enemy.sp--;
        cout << "\nMusuh memulihkan " << enemy.heal << " HP!";
    } else if (enemy.sp >= 2) {
        int dmg = rollAttack(DICE_SKILL2, enemy, player, "Dadu musuh");
        player.hp -= dmg;
        enemy.sp--;
        cout << "\nMusuh memberikan " << dmg << " dmg!";
    } else {
        int dmg = rollAttack(DICE_SKILL1, enemy, player, "Dadu musuh");
        player.hp -= dmg;
        enemy.sp++;
        cout << "\nMusuh memberikan " << dmg << " dmg!";
    }
}

// Ringkasan kekuatan player vs perkiraan musuh
void displayBriefing(const Stats &stats, const vector<Gear> &gear, const Invader &inv, double ease) {
    int critBase  = static_cast<int>(calcPlayerCrit(stats) * 100 + 0.5);
    int critBonus = gearBonus(gear, GEAR_CRIT);
    int defBonus  = gearBonus(gear, GEAR_DEF);
    int penBonus  = gearBonus(gear, GEAR_PEN);
    int lowGuess  = static_cast<int>(inv.expectedPower * (1.0 - ENEMY_POWER_VARIANCE));
    int highGuess = static_cast<int>(inv.expectedPower * (1.0 + ENEMY_POWER_VARIANCE));
 
    cout << "Kekuatan Anda  : " << calcPower(stats) << " | Crit " << critBase << "%";
    if (critBonus > 0) cout << " (+" << critBonus << ")";
    cout << " | Def " << BASE_DEF;
    if (defBonus > 0) cout << " (+" << defBonus << ")";
    cout << " | Pen " << BASE_PEN;
    if (penBonus > 0) cout << " (+" << penBonus << ")";
    cout << "\n";
 
    if (ease > 0) {
        cout << "(Musuh masih kelelahan dari perang sebelumnya)\n";
    }
    cout << "Kekuatan Musuh : sekitar " << lowGuess << " - " << highGuess
         << " | Crit " << static_cast<int>(inv.critRate * 100 + 0.5) << "%"
         << " | Def ~" << inv.expectedDef << " | Pen ~" << inv.expectedPen
         << " (perkiraan mata-mata)\n";
}

// Beli, barang yang dah dibeli hilang dari toko (sampai musuh berikutnya dateng)
void shopBuy(Stats &stats, vector<Gear> &gear, vector<ShopItem> &shop) {
    while (true) {
        cout << "\n" << LINE << "\n";
        cout << "              TOKO PERLENGKAPAN\n";
        cout << LINE << "\n";
        cout << "Gold anda: " << stats.gold << "\n";
        cout << "Perlengkapan: " << gear.size() << "/" << MAX_GEAR << "\n";
 
        if (static_cast<int>(gear.size()) >= MAX_GEAR) {
            cout << "Perlengkapan penuh! Jual salah satu dulu kalau mau beli yang baru.\n";
            return;
        }
 
        if (shop.empty()) {
            cout << "Semua perlengkapan sudah terbeli.\n";
            return;
        }
 
        for (size_t i = 0; i < shop.size(); i++) {
            cout << i + 1 << ". " << gearLabel(shop[i].gear) << " - " << shop[i].price << " Gold";
            if (shop[i].price >= stats.gold) {
                cout << " [Gold tidak cukup]";
            }
            cout << "\n";
        }
        cout << "0. Kembali\n";
 
        int pick = askNumber("Pilih barang (0 = kembali): ", 0, static_cast<int>(shop.size()));
        if (pick == 0) {
            return;
        }
 
        ShopItem item = shop[pick - 1];
        if (item.price >= stats.gold) { // sisa gold harus tetap di atas 0
            cout << "Gold tidak cukup!\n";
            continue;
        }
 
        stats.gold -= item.price;
        gear.push_back(item.gear);
        shop.erase(shop.begin() + (pick - 1));
        cout << "Anda membeli " << gearLabel(item.gear) << " seharga " << item.price << " Gold.\n";
    }
}

// Jual
void shopSell(Stats &stats, vector<Gear> &gear) {
    while (true) {
        cout << "\n" << LINE << "\n";
        cout << "              JUAL PERLENGKAPAN\n";
        cout << LINE << "\n";
        cout << "Gold anda: " << stats.gold << "\n";
        cout << "Perlengkapan: " << gear.size() << "/" << MAX_GEAR << "\n";
 
        if (gear.empty()) {
            cout << "Anda belum punya perlengkapan.\n";
            return;
        }
 
        for (size_t i = 0; i < gear.size(); i++) {
            cout << i + 1 << ". " << gearLabel(gear[i]) << " - dijual " << calcSellPrice(gear[i]) << " Gold\n";
        }
        cout << "0. Kembali\n";
 
        int pick = askNumber("Pilih barang (0 = kembali): ", 0, static_cast<int>(gear.size()));
        if (pick == 0) {
            return;
        }
 
        Gear item = gear[pick - 1];
        int price = calcSellPrice(item);
        stats.gold += price;
        gear.erase(gear.begin() + (pick - 1));
        cout << "Anda menjual " << gearLabel(item) << " seharga " << price << " Gold.\n";
    }
}

void handlePreparation(Stats &stats, vector<Gear> &gear, vector<ShopItem> &shop, const Invader &inv, double ease) {
    while (true) {
        cout << "\n" << LINE << "\n";
        cout << "                  PERSIAPAN\n";
        cout << LINE << "\n";
        displayBriefing(stats, gear, inv, ease);
        cout << LINE << "\n";
        cout << "Perlengkapan tambahan anda (" << gear.size() << "/" << MAX_GEAR << "):\n";
        if (gear.empty()) {
            cout << "(belum ada)\n";
        }
        for (size_t i = 0; i < gear.size(); i++) {
            cout << i + 1 << ". " << gearLabel(gear[i]) << "\n";
        }
        cout << LINE << "\n";
        cout << "1. Belanja (Hati hati, penggunaan gold mempengaruhi power anda!)\n";
        cout << "2. Jual (Menjual perlengkapan untuk gold bisa menambah power!)\n";
        cout << "3. Keluar\n";
 
        char choice = askChar("Pilih (1/2/3): ", "123", "Input tidak valid! Pilih 1,2, atau 3");
        if (choice == '1') {
            shopBuy(stats, gear, shop);
        } else if (choice == '2') {
            shopSell(stats, gear);
        } else {
            return;
        }
    }
}

void handleBattle(int cycle, Stats &stats, vector<Gear> &gear, bool &quit, double &ease) {
    Invader invader = makeInvader(cycle, calcPower(stats), ease); // Ditentukan sekali, sebelum belanja
    vector<ShopItem> shop = generateShop(cycle); // Toko baru tiap penjajah datang
    int tribute = calcTribute(invader.expectedPower);
 
    cout << "\n" << LINE << "\n";
    cout << "                 PENJAJAH DATANG!!!\n";
    cout << "               lindungi kekaisaran anda\n";
 
    char decision;
    while (true) {
        bool canPay = stats.gold > tribute;
 
        cout << LINE << "\n";
        displayBriefing(stats, gear, invader, ease);
        cout << LINE << "\n";
        cout << "1. Hadapi mereka (menang dapat rampasan, kalah kehilangan banyak)\n";
        cout << "2. Bayar upeti : -" << tribute << " Gold (tanpa pertempuran, tanpa rampasan)";
        if (!canPay) {
            cout << " [Gold tidak cukup]";
        }
        cout << "\n";
        cout << "3. Persiapan (beli/jual perlengkapan)\n";
 
        decision = askChar("Pilih (1/2/3): ", canPay ? "123" : "13", "Input tidak valid!");
        if (decision == '3') {
            handlePreparation(stats, gear, shop, invader, ease);
            continue;
        }
        break;
    }
 
    if (decision == '2') {
        stats.gold -= tribute;
        cout << "\nAnda membayar upeti sebesar " << tribute << " Gold. Penjajah pergi tanpa pertempuran.\n";
        waitForEnter("Tekan Enter Untuk Lanjut...\n");
        return;
    }
 
    Fighter player = makePlayerFighter(stats, gear);
    Fighter enemy  = makeFighter(invader.power, invader.critRate, invader.def, invader.pen);
    double powerRatio = static_cast<double>(enemy.power) / player.power; // Buat hukuman kalah
 
    while (true) {
        displayCombatInfo(player, enemy);
 
        char action = askCombatAction(player.sp);
        if (action == 'q') {
            cout << "Apakah anda yakin untuk menyerah?\n";
            if (askConfirm() == 'y') {
                quit = true;
                break;
            }
            continue;
        }
 
        if (action == '4') {
            cout << "\n" << LINE << "\n";
            cout << "                 Anda Mundur\n";
            cout << "        Pasukan ditarik dari medan perang\n";
            cout << LINE << "\n";
            LossRates retreat = calcRetreatRates(powerRatio);
            reduceStats(stats, retreat.gold, retreat.population, retreat.happiness);
            cout << "Kerugian: semua stat -" << static_cast<int>(retreat.gold * 100) << "%\n";
            break;
        }
 
        playerAction(action, player, enemy);
 
        if (enemy.hp <= 0) {
            cout << "\n" << LINE << "\n";
            cout << "                 Anda Menang\n";
            cout << "      Penajajah bergabung dan Mendapat Harta\n";
            cout << LINE << "\n";
 
            Loot loot = calcLoot(enemy.power);
            int happinessGain = max(0, min(loot.happiness, MAX_HAPPINESS - stats.happiness));
            cout << "Pilih rampasan:\n";
            cout << "1. Harta    : +" << loot.gold << " Gold\n";
            cout << "2. Rekrut   : +" << loot.population << " Populasi\n";
            cout << "3. Perayaan : +" << happinessGain << " Kebahagiaan (crit +" << static_cast<int>(happinessGain * CRIT_PER_HAPPINESS)
                 << "%, maks " << MAX_HAPPINESS << "%)\n";
            char pick = askChar("Pilih (1/2/3): ", "123", "Input tidak valid! Pilih 1,2, atau 3");
            applyLoot(stats, pick, loot);
            ease = 0; // Msuh normal lagi
            break;
        }
 
        enemyAction(enemy, player);
 
        if (player.hp <= 0) {
            cout << "\n" << LINE << "\n";
            cout << "                  Anda Kalah\n";
            cout << "       Anda Kehilangan Penduduk dan Harta\n";
            cout << LINE << "\n";
            LossRates loss = calcLossRates(powerRatio);
            reduceStats(stats, loss.gold, loss.population, loss.happiness);
            cout << "Kerugian: Gold -" << static_cast<int>(loss.gold * 100) << "% | Populasi -"
                 << static_cast<int>(loss.population * 100) << "% | Kebahagiaan -"
                 << static_cast<int>(loss.happiness * 100) << "%\n";
            ease = CATCH_UP_EASE; // Musuh berikutnya lebih lemah
            break;
        }
    }
 
    waitForEnter("Tekan Enter Untuk Lanjut...\n");
}


// Flow
bool isAlive(const Stats &stats) {
    return stats.gold > 0 && stats.population > 0 && stats.happiness > 0;
}
 
void displayGameOver(const Stats &stats, int cycle, bool quit) {
    cout << "\n" << LINE << "\n";
    cout << "                  GAME OVER\n";
    cout << LINE << "\n";
    cout << "                STATUS KEKAISARAN\n";
    cout << "Gold               : " << stats.gold << "\n";
    cout << "Populasi           : " << stats.population << "\n";
    cout << "Tingkat Kebahagiaan: " << stats.happiness << "%\n";
    cout << LINE << "\n";
    cout << "Total Pengunjung: " << cycle << "\n";
 
    if (stats.gold <= 0) {
        cout << "Anda bangkrut\n";
    } else if (stats.population <= 0) {
        cout << "Anda Kehilangan Rakyat\n";
    } else if (stats.happiness <= 0) {
        cout << "Rakyat Anda Tidak Bahagia dan Memberontak\n";
    } else if (quit) {
        cout << "Anda Melantarkan Kekaisaran Anda\n";
    }
}

// Highscore
int playGame(Stats stats) {
    EventDeck deck;
    int cycle = 0;
    bool quit = false;
    double ease = 0; // Pengurang power musuh setelah kalah (catch-up)
    vector<Gear> gear; // Perlengkapan yang dimiliki (permanen, tidak hilang saat kalah)
 
    while (true) {
        cycle++;
        displayStatus(stats, cycle);
        handleEvent(deck.draw(), stats, quit);
 
        if (!quit) {
            waitForEnter("Tekan enter untuk lanjut...");
        }
 
        if (!quit && cycle % BATTLE_INTERVAL == 0 && isAlive(stats)) {
            handleBattle(cycle, stats, gear, quit, ease);
        }
 
        if (quit || !isAlive(stats)) {
            displayGameOver(stats, cycle, quit);
            return cycle;
        }
    }
}

int main() {
    srand(static_cast<unsigned int>(time(nullptr)));
 
    int highScore = 0;
    bool playing = true;
 
    while (playing) {
        displayMainMenu(highScore);
 
        Stats startingStats = selectDifficulty();
        int score = playGame(startingStats);
        highScore = max(highScore, score);
 
        if (askPlayAgain() == 'n') {
            playing = false;
            cout << "\n" << LINE << "\n";
            cout << "             Terimakasih telah bermain\n";
            cout << LINE << "\n";
        }
    }
 
    return 0;
}