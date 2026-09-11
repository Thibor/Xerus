#include <iostream>
#include <sstream> 
#include <random>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

using namespace std;

#define MAX_PLY 64
#define INF 32001
#define MATE 32000
#define U8 unsigned __int8
#define S16 signed __int16
#define U16 unsigned __int16
#define S32 signed __int32
#define S64 signed __int64
#define U64 unsigned __int64
#define NAME "Xerus"
#define VERSION "2026-04-16"
#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
#define FLIP(sq) ((sq)^56)

enum Color { WHITE, BLACK, COLOR_NB };
enum PieceType { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, PT_NB };
enum Bound { UPPER, LOWER, EXACT };

struct Position {
	bool flipped = false;
	U8 move50;
	U64 castling[4];
	U64 color[2];
	U64 pieces[6];
	U64 ep;
};

struct Move {
	U8 from = 0;
	U8 to = 0;
	U8 promo = 0;
};

const Move no_move{};

struct Stack {
	Move moves[256];
	Move moves_evaluated[256];
	S64 moves_scores[256];
	Move move;
	Move killer;
	S16 score;
};

struct TT_Entry {
	U64 key;
	Move move;
	U8 flag;
	S16 score;
	U8 depth;
};

struct SearchInfo {
	bool post;
	bool stop;
	int depthLimit;
	U64 timeStart;
	U64 timeLimit;
	U64 nodes;
	U64 nodesLimit;
}info;

static const U64 FILE_A = 0x0101010101010101ULL;
static const U64 FILE_B = 0x0202020202020202ULL;
static const U64 FILE_C = 0x0404040404040404ULL;
static const U64 FILE_D = 0x0808080808080808ULL;
static const U64 FILE_E = 0x1010101010101010ULL;
static const U64 FILE_F = 0x2020202020202020ULL;
static const U64 FILE_G = 0x4040404040404040ULL;
static const U64 FILE_H = 0x8080808080808080ULL;

static const U64 FILE_N_A = ~FILE_A;
static const U64 FILE_N_H = ~FILE_H;

U64 ranksBB[8] = {
	0x00000000000000ffULL,
	0x000000000000ff00ULL,
	0x0000000000ff0000ULL,
	0x00000000ff000000ULL,
	0x000000ff00000000ULL,
	0x0000ff0000000000ULL,
	0x00ff000000000000ULL,
	0xff00000000000000ULL };

U64 filesBB[8] = { FILE_A,FILE_B,FILE_C,FILE_D,FILE_E,FILE_F,FILE_G,FILE_H };

const int phases[PT_NB] = { 0, 1, 1, 2, 4, 0 };
const int insVal[PT_NB] = { 5, 2, 3, 5, 5, 0 };
int mg_value[6] = { 82, 337, 365, 477, 1025, 0 };
int eg_value[6] = { 94, 281, 297, 512,  936, 0 };
//int mx_value[6] = { 94, 337, 365, 512, 1025, 0 };
U64 bbSquare[64];
U64 bbKnightAttack[64];
U64 bbKingAttack[64];

int mg_pawn_table[64] = {
	  0,   0,   0,   0,   0,   0,  0,   0,
	 98, 134,  61,  95,  68, 126, 34, -11,
	 -6,   7,  26,  31,  65,  56, 25, -20,
	-14,  13,   6,  21,  23,  12, 17, -23,
	-27,  -2,  -5,  12,  17,   6, 10, -25,
	-26,  -4,  -4, -10,   3,   3, 33, -12,
	-35,  -1, -20, -23, -15,  24, 38, -22,
	  0,   0,   0,   0,   0,   0,  0,   0,
};

int eg_pawn_table[64] = {
	  0,   0,   0,   0,   0,   0,   0,   0,
	178, 173, 158, 134, 147, 132, 165, 187,
	 94, 100,  85,  67,  56,  53,  82,  84,
	 32,  24,  13,   5,  -2,   4,  17,  17,
	 13,   9,  -3,  -7,  -7,  -8,   3,  -1,
	  4,   7,  -6,   1,   0,  -5,  -1,  -8,
	 13,   8,   8,  10,  13,   0,   2,  -7,
	  0,   0,   0,   0,   0,   0,   0,   0,
};

int mg_knight_table[64] = {
	-167, -89, -34, -49,  61, -97, -15, -107,
	 -73, -41,  72,  36,  23,  62,   7,  -17,
	 -47,  60,  37,  65,  84, 129,  73,   44,
	  -9,  17,  19,  53,  37,  69,  18,   22,
	 -13,   4,  16,  13,  28,  19,  21,   -8,
	 -23,  -9,  12,  10,  19,  17,  25,  -16,
	 -29, -53, -12,  -3,  -1,  18, -14,  -19,
	-105, -21, -58, -33, -17, -28, -19,  -23,
};

int eg_knight_table[64] = {
	-58, -38, -13, -28, -31, -27, -63, -99,
	-25,  -8, -25,  -2,  -9, -25, -24, -52,
	-24, -20,  10,   9,  -1,  -9, -19, -41,
	-17,   3,  22,  22,  22,  11,   8, -18,
	-18,  -6,  16,  25,  16,  17,   4, -18,
	-23,  -3,  -1,  15,  10,  -3, -20, -22,
	-42, -20, -10,  -5,  -2, -20, -23, -44,
	-29, -51, -23, -15, -22, -18, -50, -64,
};

int mg_bishop_table[64] = {
	-29,   4, -82, -37, -25, -42,   7,  -8,
	-26,  16, -18, -13,  30,  59,  18, -47,
	-16,  37,  43,  40,  35,  50,  37,  -2,
	 -4,   5,  19,  50,  37,  37,   7,  -2,
	 -6,  13,  13,  26,  34,  12,  10,   4,
	  0,  15,  15,  15,  14,  27,  18,  10,
	  4,  15,  16,   0,   7,  21,  33,   1,
	-33,  -3, -14, -21, -13, -12, -39, -21,
};

int eg_bishop_table[64] = {
	-14, -21, -11,  -8, -7,  -9, -17, -24,
	 -8,  -4,   7, -12, -3, -13,  -4, -14,
	  2,  -8,   0,  -1, -2,   6,   0,   4,
	 -3,   9,  12,   9, 14,  10,   3,   2,
	 -6,   3,  13,  19,  7,  10,  -3,  -9,
	-12,  -3,   8,  10, 13,   3,  -7, -15,
	-14, -18,  -7,  -1,  4,  -9, -15, -27,
	-23,  -9, -23,  -5, -9, -16,  -5, -17,
};

int mg_rook_table[64] = {
	 32,  42,  32,  51, 63,  9,  31,  43,
	 27,  32,  58,  62, 80, 67,  26,  44,
	 -5,  19,  26,  36, 17, 45,  61,  16,
	-24, -11,   7,  26, 24, 35,  -8, -20,
	-36, -26, -12,  -1,  9, -7,   6, -23,
	-45, -25, -16, -17,  3,  0,  -5, -33,
	-44, -16, -20,  -9, -1, 11,  -6, -71,
	-19, -13,   1,  17, 16,  7, -37, -26,
};

int eg_rook_table[64] = {
	13, 10, 18, 15, 12,  12,   8,   5,
	11, 13, 13, 11, -3,   3,   8,   3,
	 7,  7,  7,  5,  4,  -3,  -5,  -3,
	 4,  3, 13,  1,  2,   1,  -1,   2,
	 3,  5,  8,  4, -5,  -6,  -8, -11,
	-4,  0, -5, -1, -7, -12,  -8, -16,
	-6, -6,  0,  2, -9,  -9, -11,  -3,
	-9,  2,  3, -1, -5, -13,   4, -20,
};

int mg_queen_table[64] = {
	-28,   0,  29,  12,  59,  44,  43,  45,
	-24, -39,  -5,   1, -16,  57,  28,  54,
	-13, -17,   7,   8,  29,  56,  47,  57,
	-27, -27, -16, -16,  -1,  17,  -2,   1,
	 -9, -26,  -9, -10,  -2,  -4,   3,  -3,
	-14,   2, -11,  -2,  -5,   2,  14,   5,
	-35,  -8,  11,   2,   8,  15,  -3,   1,
	 -1, -18,  -9,  10, -15, -25, -31, -50,
};

int eg_queen_table[64] = {
	 -9,  22,  22,  27,  27,  19,  10,  20,
	-17,  20,  32,  41,  58,  25,  30,   0,
	-20,   6,   9,  49,  47,  35,  19,   9,
	  3,  22,  24,  45,  57,  40,  57,  36,
	-18,  28,  19,  47,  31,  34,  39,  23,
	-16, -27,  15,   6,   9,  17,  10,   5,
	-22, -23, -30, -16, -16, -23, -36, -32,
	-33, -28, -22, -43,  -5, -32, -20, -41,
};

int mg_king_table[64] = {
	-65,  23,  16, -15, -56, -34,   2,  13,
	 29,  -1, -20,  -7,  -8,  -4, -38, -29,
	 -9,  24,   2, -16, -20,   6,  22, -22,
	-17, -20, -12, -27, -30, -25, -14, -36,
	-49,  -1, -27, -39, -46, -44, -33, -51,
	-14, -14, -22, -46, -44, -30, -15, -27,
	  1,   7,  -8, -64, -43, -16,   9,   8,
	-15,  36,  12, -54,   8, -28,  24,  14,
};

int eg_king_table[64] = {
	-74, -35, -18, -18, -11,  15,   4, -17,
	-12,  17,  14,  17,  17,  38,  23,  11,
	 10,  17,  23,  15,  20,  45,  44,  13,
	 -8,  22,  24,  27,  26,  33,  26,   3,
	-18,  -4,  21,  24,  27,  23,   9, -11,
	-19,  -3,  11,  21,  23,  16,   7,  -9,
	-27, -11,   4,  13,  14,   4,  -5, -17,
	-53, -34, -21, -11, -28, -14, -24, -43
};

int* mg_table[6] = {
	mg_pawn_table,
	mg_knight_table,
	mg_bishop_table,
	mg_rook_table,
	mg_queen_table,
	mg_king_table
};

int* eg_table[6] = {
	eg_pawn_table,
	eg_knight_table,
	eg_bishop_table,
	eg_rook_table,
	eg_queen_table,
	eg_king_table
};

U64 rook_magic[64] = {
	0x8a80104000800020ULL, 0x0140002000100040ULL, 0x02801880a0017001ULL, 0x0100081001000420ULL,
	0x0200020010080420ULL, 0x03001c0002010008ULL, 0x8480008002000100ULL, 0x2080088004402900ULL,
	0x0000800098204000ULL, 0x2024401000200040ULL, 0x0100802000801000ULL, 0x0120800800801000ULL,
	0x0208808088000400ULL, 0x0002802200800400ULL, 0x2200800100020080ULL, 0x0801000060821100ULL,
	0x0080044006422000ULL, 0x0100808020004000ULL, 0x12108a0010204200ULL, 0x0140848010000802ULL,
	0x0481828014002800ULL, 0x8094004002004100ULL, 0x4010040010010802ULL, 0x0000020008806104ULL,
	0x0100400080208000ULL, 0x2040002120081000ULL, 0x0021200680100081ULL, 0x0020100080080080ULL,
	0x0002000a00200410ULL, 0x0000020080800400ULL, 0x0080088400100102ULL, 0x0080004600042881ULL,
	0x4040008040800020ULL, 0x0440003000200801ULL, 0x0004200011004500ULL, 0x0188020010100100ULL,
	0x0014800401802800ULL, 0x2080040080800200ULL, 0x0124080204001001ULL, 0x0200046502000484ULL,
	0x0480400080088020ULL, 0x1000422010034000ULL, 0x0030200100110040ULL, 0x0000100021010009ULL,
	0x2002080100110004ULL, 0x0202008004008002ULL, 0x0020020004010100ULL, 0x2048440040820001ULL,
	0x0101002200408200ULL, 0x0040802000401080ULL, 0x4008142004410100ULL, 0x02060820c0120200ULL,
	0x0001001004080100ULL, 0x020c020080040080ULL, 0x2935610830022400ULL, 0x0044440041009200ULL,
	0x0280001040802101ULL, 0x2100190040002085ULL, 0x80c0084100102001ULL, 0x4024081001000421ULL,
	0x00020030a0244872ULL, 0x0012001008414402ULL, 0x02006104900a0804ULL, 0x0001004081002402ULL
};
U64 bishop_magic[64] = {
	0x0040040844404084ULL, 0x002004208a004208ULL, 0x0010190041080202ULL, 0x0108060845042010ULL,
	0x0581104180800210ULL, 0x2112080446200010ULL, 0x1080820820060210ULL, 0x03c0808410220200ULL,
	0x0004050404440404ULL, 0x0000021001420088ULL, 0x24d0080801082102ULL, 0x0001020a0a020400ULL,
	0x0000040308200402ULL, 0x0004011002100800ULL, 0x0401484104104005ULL, 0x0801010402020200ULL,
	0x00400210c3880100ULL, 0x0404022024108200ULL, 0x0810018200204102ULL, 0x0004002801a02003ULL,
	0x0085040820080400ULL, 0x810102c808880400ULL, 0x000e900410884800ULL, 0x8002020480840102ULL,
	0x0220200865090201ULL, 0x2010100a02021202ULL, 0x0152048408022401ULL, 0x0020080002081110ULL,
	0x4001001021004000ULL, 0x800040400a011002ULL, 0x00e4004081011002ULL, 0x001c004001012080ULL,
	0x8004200962a00220ULL, 0x8422100208500202ULL, 0x2000402200300c08ULL, 0x8646020080080080ULL,
	0x80020a0200100808ULL, 0x2010004880111000ULL, 0x623000a080011400ULL, 0x42008c0340209202ULL,
	0x0209188240001000ULL, 0x400408a884001800ULL, 0x00110400a6080400ULL, 0x1840060a44020800ULL,
	0x0090080104000041ULL, 0x0201011000808101ULL, 0x1a2208080504f080ULL, 0x8012020600211212ULL,
	0x0500861011240000ULL, 0x0180806108200800ULL, 0x4000020e01040044ULL, 0x300000261044000aULL,
	0x0802241102020002ULL, 0x0020906061210001ULL, 0x5a84841004010310ULL, 0x0004010801011c04ULL,
	0x000a010109502200ULL, 0x0000004a02012000ULL, 0x500201010098b028ULL, 0x8040002811040900ULL,
	0x0028000010020204ULL, 0x06000020202d0240ULL, 0x8918844842082200ULL, 0x4010011029020020ULL
};

static const int rook_relevant_bits[64] = {
	12,11,11,11,11,11,11,12,
	11,10,10,10,10,10,10,11,
	11,10,10,10,10,10,10,11,
	11,10,10,10,10,10,10,11,
	11,10,10,10,10,10,10,11,
	11,10,10,10,10,10,10,11,
	11,10,10,10,10,10,10,11,
	12,11,11,11,11,11,11,12
};

static const int bishop_relevant_bits[64] = {
	6,5,5,5,5,5,5,6,
	5,5,5,5,5,5,5,5,
	5,5,7,7,7,7,5,5,
	5,5,7,9,9,7,5,5,
	5,5,7,9,9,7,5,5,
	5,5,7,7,7,7,5,5,
	5,5,5,5,5,5,5,5,
	6,5,5,5,5,5,5,6
};

static U64 rookMask[64];
static U64 bishopMask[64];
static vector<U64> rookAttacks[64];
static vector<U64> bishopAttacks[64];

int mg_pst[PT_NB][64];
int eg_pst[PT_NB][64];

const U64 tt_count = 64ULL << 15;
int material[7] = { 100,320,330,500,900,0,0 };
U64 keys[848];
Stack stack[MAX_PLY]{};
S32 hh_table[2][2][64][64]{};
TT_Entry tt[tt_count]{};
int hash_count = 0;
U64 hash_history[1024]{};

void UciCommand(Position& pos, string command);

static bool operator==(const Move& lhs, const Move& rhs) { return !memcmp(&rhs, &lhs, sizeof(Move)); }

static inline int LSB(const U64 bb) { return (int)_tzcnt_u64(bb); }
static inline int Count(const U64 bb) { return (int)_mm_popcnt_u64(bb); }
static inline U64 East(const U64 bb) { return (bb << 1) & FILE_N_A; }
static inline U64 West(const U64 bb) { return (bb >> 1) & FILE_N_H; }
static inline U64 North(const U64 bb) { return bb << 8; }
static inline U64 South(const U64 bb) { return bb >> 8; }
static inline U64 NW(const U64 bb) { return (bb << 7) & FILE_N_H; }
static inline U64 NE(const U64 bb) { return (bb << 9) & FILE_N_A; }
static inline U64 SW(const U64 bb) { return (bb >> 9) & FILE_N_H; }
static inline U64 SE(const U64 bb) { return (bb >> 7) & FILE_N_A; }
static inline void TTClear() { memset(tt, 0, sizeof(tt)); }
static inline U64 GetTimeMs() { return GetTickCount64(); }
static inline U64 Flip(const U64 bb) { return _byteswap_uint64(bb); }

static void InitEval() {
	for (int pt = PAWN; pt <= KING; pt++) {
		for (int sq = 0; sq < 64; sq++) {
			mg_pst[pt][sq] = mg_value[pt] + mg_table[pt][FLIP(sq)];
			eg_pst[pt][sq] = eg_value[pt] + eg_table[pt][FLIP(sq)];
		}
	}
}

static bool IsRepetition(Position& pos, U64 hash) {
	int limit = max(0, hash_count - pos.move50);
	for (int n = hash_count - 4; n >= limit; n -= 2)
		if (hash_history[n] == hash)
			return true;
	return false;
}

static void FlipPosition(Position& pos) {
	pos.color[0] = Flip(pos.color[0]);
	pos.color[1] = Flip(pos.color[1]);
	for (int i = 0; i < 6; ++i)
		pos.pieces[i] = Flip(pos.pieces[i]);
	pos.ep = Flip(pos.ep);
	swap(pos.color[0], pos.color[1]);
	swap(pos.castling[0], pos.castling[2]);
	swap(pos.castling[1], pos.castling[3]);
	pos.flipped = !pos.flipped;
}

static string SquareToUci(const int sq, const int flip) {
	string str;
	str += 'a' + (sq % 8);
	str += '1' + (flip ? (7 - sq / 8) : (sq / 8));
	return str;
}

static string MoveToUci(const Move& move, const int flip) {
	string str = SquareToUci(move.from, flip) + SquareToUci(move.to, flip);
	if (move.promo != PT_NB)
		str += "\0nbrq\0\0"[move.promo];
	return str;
}

static Move UciToMove(string& uci, int flip) {
	Move m;
	m.from = (uci[0] - 'a');
	int f = (uci[1] - '1');
	m.from += 8 * (flip ? 7 - f : f);
	m.to = (uci[2] - 'a');
	f = (uci[3] - '1');
	m.to += 8 * (flip ? 7 - f : f);
	m.promo = PT_NB;
	switch (uci[4]) {
	case 'N':
	case 'n':
		m.promo = KNIGHT;
		break;
	case 'B':
	case 'b':
		m.promo = BISHOP;
		break;
	case 'R':
	case 'r':
		m.promo = ROOK;
		break;
	case 'Q':
	case 'q':
		m.promo = QUEEN;
		break;
	}
	return m;
}

static int PieceTypeOn(const Position& pos, const int sq) {
	const U64 bb = 1ULL << sq;
	for (int i = 0; i < PT_NB; ++i) {
		if (pos.pieces[i] & bb) {
			return i;
		}
	}
	return PT_NB;
}

static void ResetInfo() {
	info.post = true;
	info.stop = false;
	info.nodes = 0;
	info.depthLimit = MAX_PLY;
	info.nodesLimit = 0;
	info.timeLimit = 0;
	info.timeStart = GetTimeMs();
}


template <typename F>
U64 Ray(const U64 bb, const U64 blockers, F f) {
	U64 mask = f(bb);
	mask |= f(mask & ~blockers);
	mask |= f(mask & ~blockers);
	mask |= f(mask & ~blockers);
	mask |= f(mask & ~blockers);
	mask |= f(mask & ~blockers);
	mask |= f(mask & ~blockers);
	mask |= f(mask & ~blockers);
	return mask;
}

static U64 KnightAttackBB(const U64 bb) {
	return (((bb << 15) | (bb >> 17)) & 0x7F7F7F7F7F7F7F7FULL) | (((bb << 17) | (bb >> 15)) & 0xFEFEFEFEFEFEFEFEULL) |
		(((bb << 10) | (bb >> 6)) & 0xFCFCFCFCFCFCFCFCULL) | (((bb << 6) | (bb >> 10)) & 0x3F3F3F3F3F3F3F3FULL);
}

static U64 KnightAttack(const int sq, U64) {
	return bbKnightAttack[sq];
}

static U64 BishopAttackBB(U64 bb, U64 blockers) {
	return Ray(bb, blockers, NW) | Ray(bb, blockers, NE) | Ray(bb, blockers, SW) | Ray(bb, blockers, SE);
}

static U64 RookAttackBB(U64 bb, U64 blockers) {
	return Ray(bb, blockers, North) | Ray(bb, blockers, East) | Ray(bb, blockers, South) | Ray(bb, blockers, West);
}

static U64 KingAttackBB(const U64 bb) {
	return (bb << 8) | (bb >> 8) | (((bb >> 1) | (bb >> 9) | (bb << 7)) & 0x7F7F7F7F7F7F7F7FULL) |
		(((bb << 1) | (bb << 9) | (bb >> 7)) & 0xFEFEFEFEFEFEFEFEULL);
}

static U64 KingAttack(const int sq, U64) {
	return bbKingAttack[sq];
}

// create mask for slider excluding edge squares
static U64 mask_rook_attacks(int sq) {
	U64 attacks = 0ULL;
	int r = sq / 8, f = sq % 8;
	// North
	for (int rr = r + 1; rr <= 6; ++rr) attacks |= (1ULL << (rr * 8 + f));
	// South
	for (int rr = r - 1; rr >= 1; --rr) attacks |= (1ULL << (rr * 8 + f));
	// East
	for (int ff = f + 1; ff <= 6; ++ff) attacks |= (1ULL << (r * 8 + ff));
	// West
	for (int ff = f - 1; ff >= 1; --ff) attacks |= (1ULL << (r * 8 + ff));
	return attacks;
}

static U64 MaskBishopAttacks(int sq) {
	U64 attacks = 0ULL;
	int r = sq / 8, f = sq % 8;
	// NE
	for (int rr = r + 1, ff = f + 1; rr <= 6 && ff <= 6; ++rr, ++ff) attacks |= (1ULL << (rr * 8 + ff));
	// NW
	for (int rr = r + 1, ff = f - 1; rr <= 6 && ff >= 1; ++rr, --ff) attacks |= (1ULL << (rr * 8 + ff));
	// SE
	for (int rr = r - 1, ff = f + 1; rr >= 1 && ff <= 6; --rr, ++ff) attacks |= (1ULL << (rr * 8 + ff));
	// SW
	for (int rr = r - 1, ff = f - 1; rr >= 1 && ff >= 1; --rr, --ff) attacks |= (1ULL << (rr * 8 + ff));
	return attacks;
}

static U64 SetOccupancy(int index, int bits_in_mask, U64 mask) {
	U64 occupancy = 0ULL;
	U64 bit = 1ULL;
	vector<int> bits;
	bits.reserve(bits_in_mask);
	U64 m = mask;
	while (m) {
		int sq = LSB(m);
		bits.push_back(sq);
		m &= m - 1;
	}
	for (int i = 0; i < bits_in_mask; ++i) {
		if (index & (1 << i))
			occupancy |= (1ULL << bits[i]);
	}
	return occupancy;
}

static void InitMagics() {
	// initialize masks and tables
	for (int sq = 0; sq < 64; ++sq) {
		bishopMask[sq] = MaskBishopAttacks(sq);
		rookMask[sq] = mask_rook_attacks(sq);

		int bbits = bishop_relevant_bits[sq];
		int rbits = rook_relevant_bits[sq];

		size_t bsize = 1ULL << bbits;
		size_t rsize = 1ULL << rbits;

		bishopAttacks[sq].assign(bsize, 0ULL);
		rookAttacks[sq].assign(rsize, 0ULL);

		// fill bishop attacks
		for (size_t index = 0; index < bsize; ++index) {
			U64 occ = SetOccupancy((int)index, bbits, bishopMask[sq]);
			U64 attack = BishopAttackBB(1ULL << sq, occ);
			U64 key = (occ * bishop_magic[sq]) >> (64 - bbits);
			bishopAttacks[sq][key] = attack;
		}
		// fill rook attacks
		for (size_t index = 0; index < rsize; ++index) {
			U64 occ = SetOccupancy((int)index, rbits, rookMask[sq]);
			U64 attack = RookAttackBB(1ULL << sq, occ);
			U64 key = (occ * rook_magic[sq]) >> (64 - rbits);
			rookAttacks[sq][key] = attack;
		}
	}
}

// New magic-based accessors
static U64 BishopAttack(int sq, const U64 blockers) {
	U64 occ = blockers & bishopMask[sq];
	int bits = bishop_relevant_bits[sq];
	U64 index = (occ * bishop_magic[sq]) >> (64 - bits);
	return bishopAttacks[sq][(size_t)index];
}

static U64 RookAttack(int sq, const U64 blockers) {
	U64 occ = blockers & rookMask[sq];
	int bits = rook_relevant_bits[sq];
	U64 index = (occ * rook_magic[sq]) >> (64 - bits);
	return rookAttacks[sq][(size_t)index];
}

static bool IsAttacked(const Position& pos, const int sq, const int them = true) {
	const U64 bb = 1ULL << sq;
	const U64 kt = pos.color[them] & pos.pieces[KNIGHT];
	const U64 BQ = pos.pieces[BISHOP] | pos.pieces[QUEEN];
	const U64 RQ = pos.pieces[ROOK] | pos.pieces[QUEEN];
	const U64 pawns = pos.color[them] & pos.pieces[PAWN];
	const U64 pawn_attacks = them ? SW(pawns) | SE(pawns) : NW(pawns) | NE(pawns);
	return (pawn_attacks & bb) | (kt & KnightAttack(sq, 0)) |
		(BishopAttack(sq, pos.color[0] | pos.color[1]) & pos.color[them] & BQ) |
		(RookAttack(sq, pos.color[0] | pos.color[1]) & pos.color[them] & RQ) |
		(KingAttack(sq, 0) & pos.color[them] & pos.pieces[KING]);
}

static bool MakeMove(Position& pos, const Move& move) {
	const int piece = PieceTypeOn(pos, move.from);
	const int captured = PieceTypeOn(pos, move.to);
	const U64 to = 1ULL << move.to;
	const U64 from = 1ULL << move.from;
	pos.move50++;
	if (captured != PT_NB || piece == PAWN)
		pos.move50 = 0;
	pos.color[0] ^= from | to;
	pos.pieces[piece] ^= from | to;
	if (piece == PAWN && to == pos.ep) {
		pos.color[1] ^= to >> 8;
		pos.pieces[PAWN] ^= to >> 8;
	}
	pos.ep = 0x0ULL;
	if (piece == PAWN && move.to - move.from == 16) {
		pos.ep = to >> 8;
	}
	if (captured != PT_NB) {
		pos.color[1] ^= to;
		pos.pieces[captured] ^= to;
	}
	if (piece == KING) {
		const U64 bb = move.to - move.from == 2 ? 0xa0ULL : move.to - move.from == -2 ? 0x9ULL : 0x0ULL;
		pos.color[0] ^= bb;
		pos.pieces[ROOK] ^= bb;
	}
	if (piece == PAWN && move.to >= 56) {
		pos.pieces[PAWN] ^= to;
		pos.pieces[move.promo] ^= to;
	}
	pos.castling[0] &= ((from | to) & 0x90ULL) == 0;
	pos.castling[1] &= ((from | to) & 0x11ULL) == 0;
	pos.castling[2] &= ((from | to) & 0x9000000000000000ULL) == 0;
	pos.castling[3] &= ((from | to) & 0x1100000000000000ULL) == 0;
	FlipPosition(pos);
	return !IsAttacked(pos, (int)LSB(pos.color[1] & pos.pieces[KING]), false);
}

static void AddMove(Move* const moveList, int& num_moves, const U8 from, const U8 to, const U8 promo = PT_NB) {
	moveList[num_moves++] = Move{ from, to, promo };
}

static void GeneratePawnMoves(Move* const moveList, int& num_moves, U64 to_mask, const int offset) {
	while (to_mask) {
		const int to = (int)LSB(to_mask);
		to_mask &= to_mask - 1;
		if (to >= 56) {
			AddMove(moveList, num_moves, to + offset, to, KNIGHT);
			AddMove(moveList, num_moves, to + offset, to, BISHOP);
			AddMove(moveList, num_moves, to + offset, to, ROOK);
			AddMove(moveList, num_moves, to + offset, to, QUEEN);
		}
		else
			AddMove(moveList, num_moves, to + offset, to);
	}
}

static void GeneratePieceMoves(Move* const moveList, int& num_moves, const Position& pos, const int piece, const U64 to_mask, U64(*func)(int, U64)) {
	U64 copy = pos.color[0] & pos.pieces[piece];
	while (copy) {
		const int fr = LSB(copy);
		copy &= copy - 1;
		U64 moves = func(fr, pos.color[0] | pos.color[1]) & to_mask;
		while (moves) {
			const int to = LSB(moves);
			moves &= moves - 1;
			AddMove(moveList, num_moves, fr, to);
		}
	}
}

static int MoveGen(const Position& pos, Move* const moveList, const bool only_captures) {
	int num_moves = 0;
	const U64 all = pos.color[0] | pos.color[1];
	const U64 to_mask = only_captures ? pos.color[1] : ~pos.color[0];
	const U64 pawns = pos.color[0] & pos.pieces[PAWN];
	GeneratePawnMoves(
		moveList, num_moves, North(pawns) & ~all & (only_captures ? 0xFF00000000000000ULL : 0xFFFFFFFFFFFF0000ULL), -8);
	if (!only_captures)
		GeneratePawnMoves(moveList, num_moves, North(North(pawns & 0xFF00ULL) & ~all) & ~all, -16);
	GeneratePawnMoves(moveList, num_moves, NW(pawns) & (pos.color[1] | pos.ep), -7);
	GeneratePawnMoves(moveList, num_moves, NE(pawns) & (pos.color[1] | pos.ep), -9);
	GeneratePieceMoves(moveList, num_moves, pos, KNIGHT, to_mask, KnightAttack);
	GeneratePieceMoves(moveList, num_moves, pos, BISHOP, to_mask, BishopAttack);
	GeneratePieceMoves(moveList, num_moves, pos, QUEEN, to_mask, BishopAttack);
	GeneratePieceMoves(moveList, num_moves, pos, ROOK, to_mask, RookAttack);
	GeneratePieceMoves(moveList, num_moves, pos, QUEEN, to_mask, RookAttack);
	GeneratePieceMoves(moveList, num_moves, pos, KING, to_mask, KingAttack);
	if (!only_captures && pos.castling[0] && !(all & 0x60ULL) && !IsAttacked(pos, 4) && !IsAttacked(pos, 5))
		AddMove(moveList, num_moves, 4, 6);
	if (!only_captures && pos.castling[1] && !(all & 0xEULL) && !IsAttacked(pos, 4) && !IsAttacked(pos, 3))
		AddMove(moveList, num_moves, 4, 2);
	return num_moves;
}

static constexpr U64 Attacks(int pt, int sq, U64 blockers) {
	switch (pt) {
	case ROOK:
		return RookAttack(sq, blockers);
	case BISHOP:
		return BishopAttack(sq, blockers);
	case QUEEN:
		return RookAttack(sq, blockers) | BishopAttack(sq, blockers);
	case KNIGHT:
		return KnightAttack(sq, blockers);
	case KING:
		return KingAttack(sq, blockers);
	default:
		return 0;
	}
}

static auto GetHash(const Position& pos) {
	U64 hash = 0;
	for (S32 p = PAWN; p < PT_NB; ++p) {
		U64 copy = pos.pieces[p] & pos.color[0];
		while (copy) {
			const S32 sq = LSB(copy);
			copy &= copy - 1;
			hash ^= keys[p * 64 + sq];
		}
		copy = pos.pieces[p] & pos.color[1];
		while (copy) {
			const S32 sq = LSB(copy);
			copy &= copy - 1;
			hash ^= keys[p * 64 + sq + 6 * 64];
		}
	}
	if (pos.ep)
		hash ^= keys[12 * 64 + LSB(pos.ep)];
	hash ^= keys[13 * 64 + pos.castling[0] + pos.castling[1] * 2 + pos.castling[2] * 4 + pos.castling[3] * 8];
	return hash;
}

static bool InputAvailable() {
	static HANDLE hstdin = 0;
	static bool pipe = false;
	unsigned long dw = 0;
	if (!hstdin) {
		hstdin = GetStdHandle(STD_INPUT_HANDLE);
		pipe = !GetConsoleMode(hstdin, &dw);
		if (!pipe)
		{
			SetConsoleMode(hstdin, dw & ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT));
			FlushConsoleInputBuffer(hstdin);
		}
		else
		{
			setvbuf(stdin, NULL, _IONBF, 0);
			setvbuf(stdout, NULL, _IONBF, 0);
		}
	}
	if (pipe)
		PeekNamedPipe(hstdin, 0, 0, 0, &dw, 0);
	else
		GetNumberOfConsoleInputEvents(hstdin, &dw);
	return dw > 1;
}

static bool CheckUp(Position& pos) {
	if ((++info.nodes & 0xffff) == 0) {
		if (info.timeLimit && GetTimeMs() - info.timeStart > info.timeLimit)
			info.stop = true;
		if (info.nodesLimit && info.nodes > info.nodesLimit)
			info.stop = true;
		if (InputAvailable()) {
			string line;
			getline(cin, line);
			UciCommand(pos, line);
		}
	}
	return info.stop;
}

static bool IsPseudolegalMove(const Position& pos, const Move& move) {
	Move moves[256];
	const int num_moves = MoveGen(pos, moves, false);
	for (int i = 0; i < num_moves; ++i)
		if (moves[i] == move)
			return true;
	return false;
}

static void PrintPv(const Position& pos, const Move move) {
	if (!IsPseudolegalMove(pos, move))
		return;
	Position npos = pos;
	if (!MakeMove(npos, move))
		return;
	cout << " " << MoveToUci(move, pos.flipped);
	const U64 tt_key = GetHash(npos);
	const TT_Entry& tt_entry = tt[tt_key % tt_count];
	if (tt_entry.key != tt_key || tt_entry.flag != EXACT)
		return;
	if (IsRepetition(npos, tt_key))
		return;
	hash_history[hash_count++] = tt_key;
	PrintPv(npos, tt_entry.move);
	hash_count--;
}

static int Popcount(const U64 bb) {
	return (int)__popcnt64(bb);
}

static int Permill() {
	int pm = 0;
	for (int n = 0; n < 1000; n++)
		if (tt[n].key)
			pm++;
	return pm;
}

//prints the bitboard
static void PrintBitboard(U64 bb) {
	const char* s = "   +---+---+---+---+---+---+---+---+\n";
	const char* t = "     A   B   C   D   E   F   G   H\n";
	cout << t;
	for (int i = 56; i >= 0; i -= 8) {
		cout << s << " " << i / 8 + 1 << " ";
		for (int x = 0; x < 8; x++) {
			const char* c = 1LL << (i + x) & bb ? "x" : " ";
			cout << "| " << c << " ";
		}
		cout << "| " << i / 8 + 1 << endl;
	}
	cout << s;
	cout << t << endl;
}

static int EvalPosition(Position& pos) {
	int phase = 0;
	int score = 0;
	int scoreMg = 0;
	int scoreEg = 0;
	int insufficient[2]{};
	U64 bbBlockers = pos.color[0] | pos.color[1];
	for (int c = WHITE; c < COLOR_NB; ++c) {
		for (int pt = PAWN; pt < KING; ++pt) {
			U64 copy = pos.color[0] & pos.pieces[pt];
			while (copy) {
				const int fr = (int)LSB(copy);
				copy &= copy - 1;
				scoreMg += mg_pst[pt][fr];
				scoreEg += eg_pst[pt][fr];
				phase += phases[pt];
				insufficient[c] += insVal[pt];
			}
		}
		U64 bbStart1 = pos.color[1] & pos.pieces[PAWN];
		U64 bbControl1 = SW(bbStart1) | SE(bbStart1);
		score -= Count(bbControl1);
		U64 bbStart0 = pos.color[0] & pos.pieces[KNIGHT];
		U64 bbAttack0 = KnightAttackBB(bbStart0) & ~bbControl1;
		score += Count(bbAttack0);
		bbStart0 = pos.color[0] & (pos.pieces[BISHOP] | pos.pieces[QUEEN]);
		bbAttack0 = BishopAttackBB(bbStart0, bbBlockers) & ~bbControl1;
		score += Count(bbAttack0);
		bbStart0 = pos.color[0] & (pos.pieces[ROOK] | pos.pieces[QUEEN]);
		bbAttack0 = RookAttackBB(bbStart0, bbBlockers) & ~bbControl1;
		score += Count(bbAttack0);
		bbStart0 = pos.color[0] & pos.pieces[KING];
		U64 file0 = filesBB[LSB(bbStart0) % 8];
		file0 |= East(file0) | West(file0);
		bbAttack0 = file0 & (ranksBB[1] | ranksBB[2]) & ~(FILE_D | FILE_E);
		bbAttack0 &= (pos.color[0] & pos.pieces[PAWN]);
		score += Count(bbAttack0);
		score += Count(bbAttack0 & ranksBB[1]);
		FlipPosition(pos);
		score = -score;
		scoreMg = -scoreMg;
		scoreEg = -scoreEg;
	}
	if (max(insufficient[0], insufficient[1]) < 5)return 0;
	if (insufficient[score < 0] < 4)return 0;
	if (phase > 24) phase = 24;
	score += (scoreMg * phase + scoreEg * (24 - phase)) / 24;
	return (100 - pos.move50) * score / 100;
}

static int SearchAlpha(Position& pos, int alpha, int beta, int depth, const int ply, Stack* const stack, const bool do_null = true) {
	if (CheckUp(pos))
		return 0;
	int  mate_value = MATE - ply;
	if (alpha < -mate_value) alpha = -mate_value;
	if (beta > mate_value - 1) beta = mate_value - 1;
	if (alpha >= beta) return alpha;
	int static_eval = EvalPosition(pos);
	if (ply >= MAX_PLY)
		return static_eval;
	stack[ply].score = static_eval;
	const S32 in_check = IsAttacked(pos, LSB(pos.color[0] & pos.pieces[KING]));
	depth += in_check;

	bool in_qsearch = depth <= 0;
	const U64 tt_key = GetHash(pos);

	if (ply > 0 && !in_qsearch)
		if (pos.move50 >= 100 || IsRepetition(pos, tt_key))
			return 0;
	TT_Entry& tt_entry = tt[tt_key % tt_count];
	Move tt_move{};
	if (tt_entry.key == tt_key) {
		tt_move = tt_entry.move;
		if (alpha == beta - 1 && tt_entry.depth >= depth) {
			if (tt_entry.flag == EXACT)
				return tt_entry.score;
			if (tt_entry.flag == LOWER && tt_entry.score <= alpha)
				return tt_entry.score;
			if (tt_entry.flag == UPPER && tt_entry.score >= beta)
				return tt_entry.score;
		}
	}
	// Internal iterative reduction
	else
		depth -= depth > 3;

	const bool improving = ply > 1 && static_eval > stack[ply - 2].score;

	// If static_eval > tt_entry.score, tt_entry.flag cannot be Lower (ie must be Upper or Exact).
	// Otherwise, tt_entry.flag cannot be Upper (ie must be Lower or Exact).
	if (tt_entry.key == tt_key && tt_entry.flag != static_eval > tt_entry.score)
		static_eval = tt_entry.score;

	if (in_qsearch && static_eval > alpha) {
		if (static_eval >= beta)
			return static_eval;
		alpha = static_eval;
	}

	if (ply > 0 && !in_qsearch && !in_check && alpha == beta - 1) {
		// Reverse futility pruning
		if (depth < 8) {
			if (static_eval - 71 * (depth - improving) >= beta)
				return static_eval;

			in_qsearch = static_eval + 238 * depth < alpha;
		}

		// Null move pruning
		if (depth > 2 && static_eval >= beta && static_eval >= stack[ply].score && do_null &&
			pos.color[0] & ~pos.pieces[PAWN] & ~pos.pieces[KING]) {
			Position npos = pos;
			FlipPosition(npos);
			npos.ep = 0;
			if (-SearchAlpha(npos,
				-beta,
				-alpha,
				depth - 4 - depth / 5 - min((static_eval - beta) / 196, 3),
				ply + 1,
				stack,
				false) >= beta)
				return beta;
		}
	}

	hash_history[hash_count++] = tt_key;
	U8 tt_flag = LOWER;

	S32 num_moves_evaluated = 0;
	S32 num_moves_quiets = 0;
	S32 best_score = in_qsearch ? static_eval : -INF;
	auto best_move = tt_move;

	auto& moves = stack[ply].moves;
	auto& moves_scores = stack[ply].moves_scores;
	auto& moves_evaluated = stack[ply].moves_evaluated;
	const S32 num_moves = MoveGen(pos, moves, in_qsearch);

	for (S32 i = 0; i < num_moves; ++i) {
		// Score moves at the first loop, except if we have a hash move,
		// then we'll use that first and delay sorting one iteration.
		if (i == !(no_move == tt_move))
			for (S32 j = 0; j < num_moves; ++j) {
				const S32 gain = material[moves[j].promo] + material[PieceTypeOn(pos, moves[j].to)];
				moves_scores[j] = hh_table[pos.flipped][!gain][moves[j].from][moves[j].to] +
					(gain || moves[j] == stack[ply].killer) * 2048 + gain;
			}

		// Find best move remaining
		S32 best_move_index = i;
		for (S32 j = i; j < num_moves; ++j) {
			if (moves[j] == tt_move) {
				best_move_index = j;
				break;
			}
			if (moves_scores[j] > moves_scores[best_move_index])
				best_move_index = j;
		}

		const Move move = moves[best_move_index];
		moves[best_move_index] = moves[i];
		moves_scores[best_move_index] = moves_scores[i];

		// Material gain
		const S32 gain = material[move.promo] + material[PieceTypeOn(pos, move.to)];

		// Delta pruning
		if (in_qsearch && !in_check && static_eval + 50 + gain < alpha)
			break;

		// Forward futility pruning
		if (ply > 0 && depth < 8 && !in_qsearch && !in_check && num_moves_evaluated && static_eval + 105 * depth + gain < alpha)
			break;

		Position npos = pos;
		if (!MakeMove(npos, move))
			continue;

		S32 score;
		S32 reduction = depth > 3 && num_moves_evaluated > 1
			? max(num_moves_evaluated / 13 + depth / 14 + (alpha == beta - 1) + !improving -
				min(max(hh_table[pos.flipped][!gain][move.from][move.to] / 128, -2), 2),
				0)
			: 0;

		while (num_moves_evaluated &&
			(score = -SearchAlpha(npos,
				-alpha - 1,
				-alpha,
				depth - reduction - 1,
				ply + 1,
				stack)) > alpha &&
			reduction > 0)
			reduction = 0;

		if (!num_moves_evaluated || score > alpha && score < beta)
			score = -SearchAlpha(npos,
				-beta,
				-alpha,
				depth - 1,
				ply + 1,
				stack);
		if (info.stop)
			break;

		if (score > best_score)
			best_score = score;

		if (score > alpha) {
			best_move = move;
			tt_flag = EXACT;
			alpha = score;
			stack[ply].move = move;
			if (!ply && info.post) {
				cout << "info depth " << depth << " score ";
				if (abs(score) < MATE - MAX_PLY)
					cout << "cp " << score;
				else
					cout << "mate " << (score > 0 ? (MATE - score + 1) >> 1 : -(MATE + score) >> 1);
				const auto elapsed = GetTimeMs() - info.timeStart;
				cout << " time " << elapsed;
				cout << " nodes " << info.nodes;
				cout << " hashfull " << Permill() << " pv";
				PrintPv(pos, stack[0].move);
				cout << endl;
			}
			if (score >= beta) {
				tt_flag = UPPER;

				if (!gain)
					stack[ply].killer = move;

				hh_table[pos.flipped][!gain][move.from][move.to] +=
					depth * depth - depth * depth * hh_table[pos.flipped][!gain][move.from][move.to] / 512;
				for (S32 j = 0; j < num_moves_evaluated; ++j) {
					const S32 prev_gain = material[moves_evaluated[j].promo] + material[PieceTypeOn(pos, moves_evaluated[j].to)];
					hh_table[pos.flipped][!prev_gain][moves_evaluated[j].from][moves_evaluated[j].to] -=
						depth * depth +
						depth * depth *
						hh_table[pos.flipped][!prev_gain][moves_evaluated[j].from][moves_evaluated[j].to] / 512;
				}
				break;
			}
		}

		moves_evaluated[num_moves_evaluated++] = move;
		if (!gain)
			num_moves_quiets++;
		if ((!in_check) && (alpha == beta - 1) && (num_moves_quiets > (1 + depth * depth) >> (int)!improving))
			break;
	}
	hash_count--;
	if (info.stop)
		return 0;
	if (best_score == -INF)
		return in_check ? ply - MATE : 0;
	tt_entry.depth = max(0, depth);
	tt_entry.flag = tt_flag;
	tt_entry.key = tt_key;
	tt_entry.move = best_move;
	tt_entry.score = best_score;
	return best_score;
}

static void SearchIteratively(Position& pos) {
	TTClear();
	int score = 0;
	int alpha = -MATE;
	int beta = MATE;
	for (int depth = 1; depth <= info.depthLimit; ++depth) {
		int aspH = 16, aspL = 16;
		do {
			if (depth > 4) {
				alpha = score - aspL;
				beta = score + aspH;
			}
			score = SearchAlpha(pos, alpha, beta, depth, 0, stack);
			if (score <= alpha) {
				alpha -= aspL;
				aspL *= 2;
			}
			else if (score >= beta) {
				beta += aspH;
				aspH *= 2;
			}
			else
				break;
		} while (!info.stop);
		if (info.stop)
			break;
		if (info.timeLimit && GetTimeMs() - info.timeStart > info.timeLimit / 2)
			break;
	}
	if (info.post)
		cout << "bestmove " << MoveToUci(stack[0].move, pos.flipped) << endl << flush;
}

static inline void PerftDriver(Position pos, int depth) {
	Move list[256];
	const S32 num_moves = MoveGen(pos, list, false);
	for (int n = 0; n < num_moves; n++) {
		Position npos = pos;
		if (!MakeMove(npos, list[n]))
			continue;
		if (depth)
			PerftDriver(npos, depth - 1);
		else
			info.nodes++;
	}
}

static void SetFen(Position& pos, const string& fen) {
	pos.flipped = false;
	pos.ep = 0;
	memset(pos.color, 0, sizeof(pos.color));
	memset(pos.pieces, 0, sizeof(pos.pieces));
	memset(pos.castling, 0, sizeof(pos.castling));
	stringstream ss(fen);
	string word;
	ss >> word;
	int i = 56;
	for (const auto c : word) {
		if (c >= '1' && c <= '8')
			i += c - '1' + 1;
		else if (c == '/')
			i -= 16;
		else {
			const int side = c == 'p' || c == 'n' || c == 'b' || c == 'r' || c == 'q' || c == 'k';
			const int piece = (c == 'p' || c == 'P') ? PAWN
				: (c == 'n' || c == 'N') ? KNIGHT
				: (c == 'b' || c == 'B') ? BISHOP
				: (c == 'r' || c == 'R') ? ROOK
				: (c == 'q' || c == 'Q') ? QUEEN
				: KING;
			pos.color[side] ^= 1ULL << i;
			pos.pieces[piece] ^= 1ULL << i;
			i++;
		}
	}
	ss >> word;
	const bool black_move = word == "b";
	ss >> word;
	for (const auto c : word) {
		pos.castling[0] |= c == 'K';
		pos.castling[1] |= c == 'Q';
		pos.castling[2] |= c == 'k';
		pos.castling[3] |= c == 'q';
	}
	ss >> word;
	if (word != "-") {
		const int sq = word[0] - 'a' + 8 * (word[1] - '1');
		pos.ep = 1ULL << sq;
	}
	ss >> word;
	pos.move50 = stoi(word);
	if (black_move)
		FlipPosition(pos);
}

static int ShrinkNumber(U64 n) {
	if (n < 10000)
		return 0;
	if (n < 10000000)
		return 1;
	if (n < 10000000000)
		return 2;
	return 3;
}

//displays a summary
static void PrintSummary(U64 time, U64 nodes) {
	U64 nps = (nodes * 1000) / max(time, 1ull);
	const char* units[] = { "", "k", "m", "g" };
	int sn = ShrinkNumber(nps);
	U64 p = (int)pow(10, sn * 3);
	printf("-----------------------------\n");
	printf("Time        : %llu\n", time);
	printf("Nodes       : %llu\n", nodes);
	printf("Nps         : %llu (%llu%s/s)\n", nps, nps / p, units[sn]);
	printf("-----------------------------\n");
}

static void PrintPerformanceHeader() {
	printf("-----------------------------\n");
	printf("ply      time        nodes\n");
	printf("-----------------------------\n");
}

//start benchmark
static void UciBench(Position& pos) {
	ResetInfo();
	PrintPerformanceHeader();
	info.depthLimit = 0;
	info.post = false;
	U64 elapsed = 0;
	while (elapsed < 3000) {
		++info.depthLimit;
		SearchIteratively(pos);
		elapsed = GetTimeMs() - info.timeStart;
		printf(" %2d. %8llu %12llu\n", info.depthLimit, elapsed, info.nodes);
	}
	PrintSummary(elapsed, info.nodes);
}

//start performance test
static void UciPerformance(Position& pos) {
	ResetInfo();
	PrintPerformanceHeader();
	info.depthLimit = 0;
	S64 elapsed = 0;
	while (elapsed < 3000) {
		PerftDriver(pos, info.depthLimit++);
		elapsed = GetTimeMs() - info.timeStart;
		printf(" %2d. %8llu %12llu\n", info.depthLimit, elapsed, info.nodes);
	}
	PrintSummary(elapsed, info.nodes);
}

static void PrintBoard(Position& pos) {
	Position npos = pos;
	if (npos.flipped)
		FlipPosition(npos);
	const char* s = "   +---+---+---+---+---+---+---+---+\n";
	const char* t = "     A   B   C   D   E   F   G   H\n";
	cout << t;
	for (int i = 56; i >= 0; i -= 8) {
		cout << s << " " << i / 8 + 1 << " ";
		for (int j = 0; j < 8; j++) {
			int sq = i + j;
			int piece = PieceTypeOn(npos, sq);
			if (npos.color[0] & 1ull << sq)
				cout << "| " << "ANBRQK "[piece] << " ";
			else
				cout << "| " << "anbrqk "[piece] << " ";
		}
		cout << "| " << i / 8 + 1 << endl;
	}
	cout << s;
	cout << t << endl;
	char castling[5] = "KQkq";
	for (int n = 0; n < 4; n++)
		if (!npos.castling[n])
			castling[n] = '-';
	printf("side     : %16s\n", pos.flipped ? "black" : "white");
	printf("castling : %16s\n", castling);
	printf("hash     : %16llx\n", GetHash(pos));
}

static void ParsePosition(Position& pos, string command) {
	string fen = START_FEN;
	stringstream ss(command);
	string token;
	ss >> token;
	if (token != "position")
		return;
	ss >> token;
	if (token == "startpos")
		ss >> token;
	else if (token == "fen") {
		fen = "";
		while (ss >> token && token != "moves")
			fen += token + " ";
		fen.pop_back();
	}
	SetFen(pos, fen);
	hash_count = 0;
	while (ss >> token) {
		Move m = UciToMove(token, pos.flipped);
		if (PieceTypeOn(pos, m.to) != PT_NB || PieceTypeOn(pos, m.from) == PAWN)
			hash_count = 0;
		hash_history[hash_count++] = GetHash(pos);
		MakeMove(pos, m);
	}
}

static void ParseGo(Position& pos, string command) {
	stringstream ss(command);
	string token;
	ss >> token;
	if (token != "go")
		return;
	ResetInfo();
	int wtime = 0;
	int btime = 0;
	int winc = 0;
	int binc = 0;
	int movestogo = 32;
	while (ss >> token) {
		if (token == "wtime")
			ss >> wtime;
		else if (token == "btime")
			ss >> btime;
		else if (token == "winc")
			ss >> winc;
		else if (token == "binc")
			ss >> binc;
		else if (token == "movestogo")
			ss >> movestogo;
		else if (token == "movetime")
			ss >> info.timeLimit;
		else if (token == "depth")
			ss >> info.depthLimit;
		else if (token == "nodes")
			ss >> info.nodesLimit;
	}
	int time = pos.flipped ? btime : wtime;
	int inc = pos.flipped ? binc : winc;
	if (time)
		info.timeLimit = max(1, min(time / movestogo + inc, time / 2));
	SearchIteratively(pos);
}

static void UciNewGame(Position& pos) {
	memset(hh_table, 0, sizeof(hh_table));
}

void UciCommand(Position& pos, string command) {
	if (command == "uci")cout << "id name " << NAME << endl << "uciok" << endl;
	else if (command == "isready")cout << "readyok" << endl;
	else if (command == "ucinewgame")UciNewGame(pos);
	else if (command == "bench")UciBench(pos);
	else if (command == "perft")UciPerformance(pos);
	else if (command == "print")PrintBoard(pos);
	else if (command == "stop")info.stop = true;
	else if (command == "quit")exit(0);
	else if (command.substr(0, 8) == "position")ParsePosition(pos, command);
	else if (command.substr(0, 2) == "go")ParseGo(pos, command);
}

static void UciLoop(Position& pos) {
	string line;
	while (true) {
		getline(cin, line);
		UciCommand(pos, line);
	}
}

static void InitBitboards() {
	for (int sq = 0; sq < 64; ++sq) {
		U64 bb = 1ULL << sq;
		bbSquare[sq] = bb;
		bbKnightAttack[sq] = KnightAttackBB(bb);
		bbKingAttack[sq] = KingAttackBB(bb);
	}
}

static void InitHash() {
	mt19937_64 r;
	for (U64& k : keys)
		k = r();
}

int main(const int argc, const char** argv) {
	Position pos;
	InitBitboards();
	InitEval();
	InitHash();
	InitMagics();
	cout << NAME << " " << VERSION << endl;
	SetFen(pos, START_FEN);
	UciLoop(pos);
}