
/*
 * Dungeon Crawler (cp2)
 * 
 * Features:
 * - Graph-based dungeon generation with adjacency lists
 * - Rooms contain monsters, items, treasure, or are empty
 * - Player navigates rooms, fights monsters with bitwise combat
 * - Items apply effects (hp restore, damage boost)
 * - Save and load game to/from file
 * - Function pointers for room entry handling
 * - Dynamic memory allocation, proper cleanup
 * - Command-line args: <num_rooms> for new game or <filename> to load
 * 
 * Compile: gcc -o dungeon_crawler dungeon_crawler.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_NEIGHBORS 4
#define BIT_ROUND_BITS 16

// Forward declarations
struct Room;
struct Player;
struct Monster;
struct Item;

typedef enum { NONE, MONSTER, ITEM, TREASURE } ContentType;
typedef enum { GOBLIN, TROLL } MonsterType;
typedef enum { POTION, SWORD } ItemType;

typedef void (*EnterFunc)(struct Room*, struct Player*);

// Linked list for neighbors
typedef struct RoomListNode {
    struct Room* room;
    struct RoomListNode* next;
} RoomListNode;

typedef struct Monster {
    MonsterType type;
    char name[16];
    int hp;
    int damage;
} Monster;

typedef struct Item {
    ItemType type;
    char name[16];
    int hp_restore;
    int damage_boost;
} Item;

typedef struct Room {
    int id;
    RoomListNode* neighbors;
    ContentType content_type;
    void* content;
    int visited;
    EnterFunc enter;
} Room;

typedef struct Player {
    Room* location;
    int hp;
    int damage;
} Player;

// Function prototypes
Room** create_dungeon(int n);
void populate_rooms(Room** rooms, int n);
RoomListNode* add_neighbor(RoomListNode* head, Room* room);
void free_dungeon(Room** rooms, int n);
void enter_empty(Room* room, Player* player);
void enter_monster(Room* room, Player* player);
void enter_item(Room* room, Player* player);
void enter_treasure(Room* room, Player* player);
void fight(Player* player, Monster* mon);
void save_game(const char* filename, Player* player, Room** rooms, int n);
Room** load_game(const char* filename, Player* player, int* out_n);
Room* find_neighbor(Room* current, int id);
void cleanup(Player* player, Room** rooms, int n);

int main(int argc, char* argv[]) {
    srand((unsigned)time(NULL));
    if (argc < 2) {
        printf("Usage: %s <num_rooms>  OR  %s <savefile>\n", argv[0], argv[0]);
        return 1;
    }
    Room** rooms = NULL;
    int n = 0;
    Player player;
    // New game if arg is integer >1
    char* endptr;
    long num = strtol(argv[1], &endptr, 10);
    if (*endptr == '\0' && num > 1) {
        n = (int)num;
        rooms = create_dungeon(n);
        populate_rooms(rooms, n);
        player.hp = 20;
        player.damage = 5;
        player.location = rooms[0];
    } else {
        // load game
        rooms = load_game(argv[1], &player, &n);
        if (!rooms) {
            fprintf(stderr, "Failed to load game from '%s'\n", argv[1]);
            return 1;
        }
    }
    printf("=== Dungeon Crawler ===\n");
    while (1) {
        Room* cur = player.location;
        printf("\nDe held staat in kamer %d\n", cur->id);
        // Check treasure
        if (cur->content_type == TREASURE) {
            enter_treasure(cur, &player);
            break;
        }
        // Trigger room content
        cur->enter(cur, &player);
        // After content, prompt move
        // List neighbors
        printf("Deuren naar kamers:");
        for (RoomListNode* it = cur->neighbors; it; it = it->next)
            printf(" %d", it->room->id);
        printf("\nKies een deur (-1 om opslaan en afsluiten): ");
        int choice;
        if (scanf("%d", &choice)!=1) break;
        if (choice == -1) {
            save_game("savegame.dat", &player, rooms, n);
            printf("Spel opgeslagen. Tot ziens!\n");
            break;
        }
        Room* dest = find_neighbor(cur, choice);
        if (dest) player.location = dest;
        else printf("Ongeldige keuze, probeer opnieuw.\n");
    }
    cleanup(&player, rooms, n);
    return 0;
}

// Create rooms and random connections ensuring connectivity
Room** create_dungeon(int n) {
    Room** rooms = malloc(sizeof(Room*) * n);
    for (int i = 0; i < n; ++i) {
        rooms[i] = malloc(sizeof(Room));
        rooms[i]->id = i;
        rooms[i]->neighbors = NULL;
        rooms[i]->content_type = NONE;
        rooms[i]->content = NULL;
        rooms[i]->visited = 0;
        rooms[i]->enter = enter_empty;
    }
    // ensure tree structure
    for (int i = 1; i < n; ++i) {
        int j = rand() % i;
        rooms[i]->neighbors = add_neighbor(rooms[i]->neighbors, rooms[j]);
        rooms[j]->neighbors = add_neighbor(rooms[j]->neighbors, rooms[i]);
    }
    // add extra random edges
    for (int i = 0; i < n; ++i) {
        int deg = 0;
        for (RoomListNode* it = rooms[i]->neighbors; it; it=it->next) deg++;
        int extras = (rand() % (MAX_NEIGHBORS - deg + 1));
        for (int e = 0; e < extras; ++e) {
            int j = rand() % n;
            if (j!=i) {
                // avoid duplicate
                int found = 0;
                for (RoomListNode* it = rooms[i]->neighbors; it; it=it->next)
                    if (it->room->id == j) found=1;
                if (!found) {
                    rooms[i]->neighbors = add_neighbor(rooms[i]->neighbors, rooms[j]);
                    rooms[j]->neighbors = add_neighbor(rooms[j]->neighbors, rooms[i]);
                }
            }
        }
    }
    return rooms;
}

void populate_rooms(Room** rooms, int n) {
    // Place treasure in random room !=0
    int treasure_room = 1 + rand() % (n-1);
    rooms[treasure_room]->content_type = TREASURE;
    rooms[treasure_room]->enter = enter_treasure;
    // Other rooms
    for (int i = 1; i < n; ++i) {
        if (i == treasure_room) continue;
        int choice = rand() % 3; // 0 empty,1 monster,2 item
        if (choice == 1) {
            rooms[i]->content_type = MONSTER;
            Monster* m = malloc(sizeof(Monster));
            if (rand()%2==0) {
                m->type = GOBLIN; strcpy(m->name, "Goblin"); m->hp = 8; m->damage = 5;
            } else {
                m->type = TROLL; strcpy(m->name, "Troll"); m->hp = 12; m->damage = 3;
            }
            rooms[i]->content = m;
            rooms[i]->enter = enter_monster;
        } else if (choice == 2) {
            rooms[i]->content_type = ITEM;
            Item* it = malloc(sizeof(Item));
            if (rand()%2==0) {
                it->type = POTION; strcpy(it->name, "Potion"); it->hp_restore = 10; it->damage_boost = 0;
            } else {
                it->type = SWORD; strcpy(it->name, "Sword"); it->hp_restore = 0; it->damage_boost = 2;
            }
            rooms[i]->content = it;
            rooms[i]->enter = enter_item;
        }
    }
}

RoomListNode* add_neighbor(RoomListNode* head, Room* room) {
    RoomListNode* node = malloc(sizeof(RoomListNode));
    node->room = room;
    node->next = head;
    return node;
}

void enter_empty(Room* room, Player* player) {
    printf("De kamer is leeg\n");
    room->visited = 1;
}

void enter_monster(Room* room, Player* player) {
    if (room->visited) { enter_empty(room, player); return; }
    Monster* m = (Monster*)room->content;
    printf("Er is een %s in de kamer! (hp:%d, dmg:%d)\n", m->name, m->hp, m->damage);
    fight(player, m);
    if (m->hp <= 0) {
        printf("%s sterft\n", m->name);
        free(m);
        room->content = NULL;
        room->content_type = NONE;
        room->enter = enter_empty;
    }
    room->visited = 1;
}

void enter_item(Room* room, Player* player) {
    if (room->visited) { enter_empty(room, player); return; }
    Item* it = (Item*)room->content;
    printf("Je vindt een %s! ", it->name);
    if (it->hp_restore) {
        player->hp += it->hp_restore;
        printf("Herstelt %d hp (nu %d hp)\n", it->hp_restore, player->hp);
    }
    if (it->damage_boost) {
        player->damage += it->damage_boost;
        printf("Damage verhoogd met %d (nu %d)\n", it->damage_boost, player->damage);
    }
    free(it);
    room->content = NULL;
    room->content_type = NONE;
    room->enter = enter_empty;
    room->visited = 1;
}

void enter_treasure(Room* room, Player* player) {
    printf("Je vindt de schat! Je wint!\n");
}

void fight(Player* player, Monster* mon) {
    // Bitwise rounds until someone dies
    while (player->hp > 0 && mon->hp > 0) {
        int rnd = rand() % (1 << BIT_ROUND_BITS);
        printf("Aanvalsvolgorde bits: ");
        for (int i = BIT_ROUND_BITS-1; i >= 0; --i)
            printf("%d", (rnd>>i)&1);
        printf("\n");
        for (int i = 0; i < BIT_ROUND_BITS && player->hp>0 && mon->hp>0; ++i) {
            int bit = (rnd >> i) & 1;
            if (bit == 0) {
                player->hp -= mon->damage;
                printf("Monstertje valt je aan voor %d dmg (hp speler %d)\n", mon->damage, player->hp);
            } else {
                mon->hp -= player->damage;
                printf("Je valt monster aan voor %d dmg (hp monster %d)\n", player->damage, mon->hp);
            }
        }
    }
    if (player->hp <= 0) {
        printf("Je bent overleden! Game over.\n");
        exit(0);
    }
}

void save_game(const char* filename, Player* player, Room** rooms, int n) {
    FILE* f = fopen(filename, "wb");
    if (!f) { perror("fopen"); return; }
    fwrite(&n, sizeof(int), 1, f);
    int cur_id = player->location->id;
    fwrite(&cur_id, sizeof(int),1,f);
    fwrite(&player->hp, sizeof(int),1,f);
    fwrite(&player->damage, sizeof(int),1,f);
    for (int i = 0; i < n; ++i) {
        Room* r = rooms[i];
        fwrite(&r->visited, sizeof(int),1,f);
        fwrite(&r->content_type, sizeof(ContentType),1,f);
        if (r->content_type == MONSTER) {
            Monster* m = (Monster*)r->content;
            fwrite(&m->type,sizeof(MonsterType),1,f);
            fwrite(&m->hp,sizeof(int),1,f);
            fwrite(&m->damage,sizeof(int),1,f);
        } else if (r->content_type == ITEM) {
            Item* it = (Item*)r->content;
            fwrite(&it->type,sizeof(ItemType),1,f);
        }
        // neighbors
        int deg = 0;
        for (RoomListNode* it = r->neighbors; it; it=it->next) deg++;
        fwrite(&deg, sizeof(int),1,f);
        for (RoomListNode* it = r->neighbors; it; it=it->next) {
            int id = it->room->id;
            fwrite(&id, sizeof(int),1,f);
        }
    }
    fclose(f);
}

Room** load_game(const char* filename, Player* player, int* out_n) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;
    int n;
    fread(&n,sizeof(int),1,f);
    *out_n = n;
    Room** rooms = malloc(sizeof(Room*)*n);
    for (int i=0;i<n;i++) {
        rooms[i] = malloc(sizeof(Room));
        rooms[i]->id = i;
        rooms[i]->neighbors = NULL;
        rooms[i]->content = NULL;
        rooms[i]->enter = enter_empty;
    }
    int cur_id, hp, dmg;
    fread(&cur_id,sizeof(int),1,f);
    fread(&hp,sizeof(int),1,f);
    fread(&dmg,sizeof(int),1,f);
    player->hp = hp;
    player->damage = dmg;
    player->location = rooms[cur_id];
    // read rooms
    for (int i=0;i<n;i++) {
        int visited;
        ContentType ct;
        fread(&visited,sizeof(int),1,f);
        rooms[i]->visited = visited;
        fread(&ct,sizeof(ContentType),1,f);
        rooms[i]->content_type = ct;
        if (ct==MONSTER) {
            Monster* m = malloc(sizeof(Monster));
            MonsterType mt; int mhp, mdmg;
            fread(&mt,sizeof(MonsterType),1,f);
            fread(&mhp,sizeof(int),1,f);
            fread(&mdmg,sizeof(int),1,f);
            m->type = mt; m->hp = mhp; m->damage = mdmg;
            strcpy(m->name, mt==GOBLIN?"Goblin":"Troll");
            rooms[i]->content = m;
            rooms[i]->enter = enter_monster;
        } else if (ct==ITEM) {
            Item* it = malloc(sizeof(Item));
            ItemType itp;
            fread(&itp,sizeof(ItemType),1,f);
            it->type = itp;
            if (itp==POTION) { strcpy(it->name,"Potion");it->hp_restore=10;it->damage_boost=0;} 
            else { strcpy(it->name,"Sword");it->hp_restore=0;it->damage_boost=2;}
            rooms[i]->content = it;
            rooms[i]->enter = enter_item;
        } else if (ct==TREASURE) {
            rooms[i]->enter = enter_treasure;
        }
        int deg;
        fread(&deg,sizeof(int),1,f);
        int* ids = malloc(sizeof(int)*deg);
        for (int j=0;j<deg;j++) fread(&ids[j],sizeof(int),1,f);
        // Temporarily store ids in content pointer if no memory; instead link after all read
        rooms[i]->content = rooms[i]->content; // keep
        for (int j=0;j<deg;j++) {
            // connect neighbors later after full read
            // use temporary array store? here we connect immediately, rooms[j] might already exist
            // but adjacency lists may duplicate edges; acceptable
            rooms[i]->neighbors = add_neighbor(rooms[i]->neighbors, rooms[ids[j]]);
        }
        free(ids);
    }
    fclose(f);
    return rooms;
}

Room* find_neighbor(Room* current, int id) {
    for (RoomListNode* it = current->neighbors; it; it=it->next)
        if (it->room->id == id) return it->room;
    return NULL;
}

void cleanup(Player* player, Room** rooms, int n) {
    for (int i = 0; i < n; ++i) {
        Room* r = rooms[i];
        // free content
        if (r->content_type == MONSTER)
            free(r->content);
        else if (r->content_type == ITEM)
            free(r->content);
        // free neighbors
        RoomListNode* it = r->neighbors;
        while (it) {
            RoomListNode* next = it->next;
            free(it);
            it = next;
        }
        free(r);
    }
    free(rooms);
}
// end{code} 