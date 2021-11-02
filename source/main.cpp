#include <iostream>
#include <set>
#include <string>
#include <cassert>
#include <cstring>
#include <algorithm>
#include <random>
#include <cmath>
#include <ctime>
#include <tuple>
#include <queue>
#include "jsoncpp/json.h" // 在平台上，C++编译时默认包含此库

using std::set;
using std::sort;
using std::unique;
using std::string;
using std::unique;
using std::vector;
using std::min;
using std::max;
using std::mt19937;
using std::pair;
using std::make_pair;
using std::tie;
using std::to_string;

mt19937 rng(time(0));

constexpr int PLAYER_COUNT = 3;

enum class Stage
{
	BIDDING, // 叫分阶段
	PLAYING	 // 打牌阶段
};

enum class CardComboType
{
	PASS,		// 过
	SINGLE,		// 单张
	PAIR,		// 对子
	STRAIGHT,	// 顺子
	STRAIGHT2,	// 双顺
	TRIPLET,	// 三条
	TRIPLET1,	// 三带一
	TRIPLET2,	// 三带二
	BOMB,		// 炸弹
	QUADRUPLE2, // 四带二（只）
	QUADRUPLE4, // 四带二（对）
	PLANE,		// 飞机
	PLANE1,		// 飞机带小翼
	PLANE2,		// 飞机带大翼
	SSHUTTLE,	// 航天飞机
	SSHUTTLE2,	// 航天飞机带小翼
	SSHUTTLE4,	// 航天飞机带大翼
	ROCKET,		// 火箭
	INVALID		// 非法牌型
};

// 牌型信息，不含火箭和炸弹
const int maxMainPacks=12,recMainPacks=6;
const vector < vector < int > > cardComboInfo[/*主牌类数*/] = { // 主牌重数，从牌类数，从牌重数
	{},
	{{1,0,0}/*单张*/,{2,0,0}/*对子*/,{3,0,0}/*三条*/,{3,1,1}/*三带一*/,{3,1,2}/*三带二*/,{4,2,1}/*四带两只*/,{4,2,2}/*四带两对*/},
	{{3,0,0}/*纯飞机*/,{3,2,1}/*飞机1*/,{3,2,2}/*飞机2*/,{4,0,0}/*纯航天飞机*/,{4,4,1}/*航天飞机1*/,{4,4,2}/*航天飞机2*/},
	{{2,0,0}/*双顺*/,{3,0,0}/*纯飞机*/,{3,3,1}/*飞机1*/,{3,3,2}/*飞机2*/},
	{{2,0,0}/*双顺*/},
	{{1,0,0}/*单顺*/,{2,0,0}/*双顺*/},
	{{1,0,0}/*单顺*/}
};

constexpr int cardComboScores[] = {
	0,	// 过
	1,	// 单张
	2,	// 对子
	6,	// 顺子
	6,	// 双顺
	4,	// 三条
	4,	// 三带一
	4,	// 三带二
	10, // 炸弹
	8,	// 四带二（只）
	8,	// 四带二（对）
	8,	// 飞机
	8,	// 飞机带小翼
	8,	// 飞机带大翼
	10, // 航天飞机（需要特判：二连为10分，多连为20分）
	10, // 航天飞机带小翼
	10, // 航天飞机带大翼
	16, // 火箭
	0	// 非法牌型
};

typedef double ProbType; // used only in the inference part
namespace PARA{ // 核心参数
	constexpr bool useHeuris=1,useInference=0;
	constexpr float c=13.274828;
	constexpr float SoverD_B=6.646546,SoverD_E=1.417460; // B * E^{ratio_of_cards_played}
	constexpr float iniWeight_B=0.015963,iniWeight_E=0.404209; // B * S * E^{ratio_of_cards_played}
	constexpr float structWeight=8.933963,playedWeight=0.421146; // weights in utility func
	constexpr ProbType utilityToProb_E=2607.789307; // E^{winProb/base}
	constexpr float inferenceTL=0.150000;
	constexpr bool default0=0;
	constexpr float myCrd_D=16.047952,myStt_D=8.880167;
	constexpr float levelWeight_E=1.145248; // card of level i has weight E^i
};
// full 1 0 13.274828 6.646546 1.417460 0.015963 0.404209 8.933963 0.421146 2607.789307 0.150000 0 16.047952 8.880167 1.145248 TiroR28

// 估价函数中矩形区域的权值
constexpr int maxRectWidth = 6;
constexpr int rectScores[6/*width-1*/][4/*height-1*/] = { // 考虑到前缀和
	{ 0, 1, 2, 5}, // w=1, h=1...4
	{ 0, 0, 2, 0}, // w=2, h=1...4
	{ 0, 2, 3}, // w=3, h=1...3
	{ 0}, // w=4, h=1
	{ 3}, // w=5, h=1
	{-1}, // w=6, h=1
};

constexpr int rectScoresPresum[6][4] = {
	{ 0, 1, 3, 8},
	{ 0, 0, 2, 2},
	{ 0, 2, 5, 5},
	{ 0, 0, 0, 0},
	{ 3, 3, 3, 3},
	{-1,-1,-1,-1}
};

// 估价函数中已出的牌的难接程度

const int maxPlayedPacks = 5, maxPlayedScore_B = 8;
constexpr float playedScore_B[5/*mainPacks*/][4/*mainCnt*/] = {
		// 该牌型最小的牌组的系数
		// (a,b) -> (a>5?8:p_B[a-1][b-1])
	{ .9,1.5,  6, 12},
	{  0,  0, 12, 12},
	{  0, 10, 12, 12},
	{  0, 12, 12, 12},
	{ 10, 12, 12, 12},
};

constexpr int playedScore_iE[5][4] = { // 指数的分母（分子是level，底数是lW_E）
									   // (a,b) -> p_iE[a-1-max(0,a-5)][b-1] + b*max(0,a-5)
	{ 1, 1, 2, 4},
	{ 0, 0, 6, 8},
	{ 0, 4, 9,12},
	{ 0, 7,12,16},
	{ 3, 9,15,20}
};

#ifndef _BOTZONE_ONLINE
string cardComboStrings[] = {
	"PASS",
	"SINGLE",
	"PAIR",
	"STRAIGHT",
	"STRAIGHT2",
	"TRIPLET",
	"TRIPLET1",
	"TRIPLET2",
	"BOMB",
	"QUADRUPLE2",
	"QUADRUPLE4",
	"PLANE",
	"PLANE1",
	"PLANE2",
	"SSHUTTLE",
	"SSHUTTLE2",
	"SSHUTTLE4",
	"ROCKET",
	"INVALID"};
#endif

// 用0~53这54个整数表示唯一的一张牌
typedef short Card;
constexpr Card card_joker = 52;
constexpr Card card_JOKER = 53;

// 除了用0~53这54个整数表示唯一的牌，
// 这里还用另一种序号表示牌的大小（不管花色），以便比较，称作等级（Level）
// 对应关系如下：
// 3 4 5 6 7 8 9 10	J Q K	A	2	小王	大王
// 0 1 2 3 4 5 6 7	8 9 10	11	12	13	14
typedef short Level;
constexpr Level MAX_LEVEL = 15;
constexpr Level MAX_STRAIGHT_LEVEL = 11;
constexpr Level level_joker = 13;
constexpr Level level_JOKER = 14;

/**
* 将Card变成Level
*/
constexpr Level card2level(Card card)
{
	return card / 4 + card / 53;
}

vector < vector < int > > subsets[15][15];

void precSubsets(){
	for(int i=0;i<=14;++i){
		for(int j=0;j<(1<<i);++j){
			vector < int > cur;
			cur.reserve(__builtin_popcount(j));
			for(int k=0;k<i;++k){
				if(j&(1<<k)) cur.push_back(k);
			}
			subsets[i][__builtin_popcount(j)].push_back(cur);
		}
	}
}

// 牌的组合，用于计算牌型
struct CardCombo
{
	// 表示同等级的牌有多少张
	// 会按个数从大到小、等级从大到小排序
	struct CardPack
	{
		Level level;
		short count;

		bool operator<(const CardPack &b) const
		{
			if (count == b.count)
				return level > b.level;
			return count > b.count;
		}
	};
	short myCounts[MAX_LEVEL + 1];
	unsigned long long cardsMask;
	vector<Card> cards;		 // 原始的牌，未排序
	vector<CardPack> packs;	 // 按数目和大小排序的牌种
	CardComboType comboType; // 算出的牌型
	Level comboLevel = 0;	 // 算出的大小序

	/**
						  * 检查个数最多的CardPack递减了几个
						  */
	int findMaxSeq() const
	{
		for (unsigned c = 1; c < packs.size(); c++)
			if (packs[c].count != packs[0].count ||
				packs[c].level != packs[c - 1].level - 1)
				return c;
		return packs.size();
	}

	/**
	* 这个牌型最后算总分的时候的权重
	*/
	int getWeight() const
	{
		if (comboType == CardComboType::SSHUTTLE ||
			comboType == CardComboType::SSHUTTLE2 ||
			comboType == CardComboType::SSHUTTLE4)
			return cardComboScores[(int)comboType] + (findMaxSeq() > 2) * 10;
		return cardComboScores[(int)comboType];
	}

	// 创建一个空牌组
	CardCombo() : myCounts{}, cardsMask(0ull), comboType(CardComboType::PASS) {}

	/**
	* 通过Card（即short）类型的迭代器创建一个牌型
	* 并计算出牌型和大小序等
	* 假设输入没有重复数字（即重复的Card）
	*/
	template <typename CARD_ITERATOR>
	CardCombo(CARD_ITERATOR begin, CARD_ITERATOR end)
	{
		// 特判：空
		if (begin == end)
		{
			comboType = CardComboType::PASS;
			memset(myCounts,0,sizeof(myCounts));
			return;
		}

		// 每种牌有多少个
		short *counts = myCounts;
		memset(myCounts,0,sizeof(myCounts));

		// 同种牌的张数（有多少个单张、对子、三条、四条）
		short countOfCount[5] = {};

		cards = vector<Card>(begin, end);
		cardsMask = 0;
		for (Card c : cards){
			counts[card2level(c)]++;
			cardsMask |= 1ull << c;
		}
		for (Level l = 0; l <= MAX_LEVEL; l++)
			if (counts[l])
			{
				packs.push_back(CardPack{l, counts[l]});
				countOfCount[counts[l]]++;
			}
		sort(packs.begin(), packs.end());

		// 用最多的那种牌总是可以比较大小的
		comboLevel = packs[0].level;

		// 计算牌型
		// 按照 同种牌的张数 有几种 进行分类
		vector<int> kindOfCountOfCount;
		for (int i = 0; i <= 4; i++)
			if (countOfCount[i])
				kindOfCountOfCount.push_back(i);
//		sort(kindOfCountOfCount.begin(), kindOfCountOfCount.end());

		int curr, lesser;

		switch (kindOfCountOfCount.size())
		{
		case 1: // 只有一类牌
			curr = countOfCount[kindOfCountOfCount[0]];
			switch (kindOfCountOfCount[0])
			{
			case 1:
				// 只有若干单张
				if (curr == 1)
				{
					comboType = CardComboType::SINGLE;
					return;
				}
				if (curr == 2 && packs[1].level == level_joker)
				{
					comboType = CardComboType::ROCKET;
					return;
				}
				if (curr >= 5 && findMaxSeq() == curr &&
					packs.begin()->level <= MAX_STRAIGHT_LEVEL)
				{
					comboType = CardComboType::STRAIGHT;
					return;
				}
				break;
			case 2:
				// 只有若干对子
				if (curr == 1)
				{
					comboType = CardComboType::PAIR;
					return;
				}
				if (curr >= 3 && findMaxSeq() == curr &&
					packs.begin()->level <= MAX_STRAIGHT_LEVEL)
				{
					comboType = CardComboType::STRAIGHT2;
					return;
				}
				break;
			case 3:
				// 只有若干三条
				if (curr == 1)
				{
					comboType = CardComboType::TRIPLET;
					return;
				}
				if (findMaxSeq() == curr &&
					packs.begin()->level <= MAX_STRAIGHT_LEVEL)
				{
					comboType = CardComboType::PLANE;
					return;
				}
				break;
			case 4:
				// 只有若干四条
				if (curr == 1)
				{
					comboType = CardComboType::BOMB;
					return;
				}
				if (findMaxSeq() == curr &&
					packs.begin()->level <= MAX_STRAIGHT_LEVEL)
				{
					comboType = CardComboType::SSHUTTLE;
					return;
				}
			}
			break;
		case 2: // 有两类牌
			curr = countOfCount[kindOfCountOfCount[1]];
			lesser = countOfCount[kindOfCountOfCount[0]];
			if (kindOfCountOfCount[1] == 3)
			{
				// 三条带？
				if (kindOfCountOfCount[0] == 1)
				{
					// 三带一
					if (curr == 1 && lesser == 1)
					{
						comboType = CardComboType::TRIPLET1;
						return;
					}
					if (findMaxSeq() == curr && lesser == curr &&
						packs.begin()->level <= MAX_STRAIGHT_LEVEL)
					{
						comboType = CardComboType::PLANE1;
						return;
					}
				}
				if (kindOfCountOfCount[0] == 2)
				{
					// 三带二
					if (curr == 1 && lesser == 1)
					{
						comboType = CardComboType::TRIPLET2;
						return;
					}
					if (findMaxSeq() == curr && lesser == curr &&
						packs.begin()->level <= MAX_STRAIGHT_LEVEL)
					{
						comboType = CardComboType::PLANE2;
						return;
					}
				}
			}
			if (kindOfCountOfCount[1] == 4)
			{
				// 四条带？
				if (kindOfCountOfCount[0] == 1)
				{
					// 四条带两只 * n
					if (curr == 1 && lesser == 2)
					{
						comboType = CardComboType::QUADRUPLE2;
						return;
					}
					if (findMaxSeq() == curr && lesser == curr * 2 &&
						packs.begin()->level <= MAX_STRAIGHT_LEVEL)
					{
						comboType = CardComboType::SSHUTTLE2;
						return;
					}
				}
				if (kindOfCountOfCount[0] == 2)
				{
					// 四条带两对 * n
					if (curr == 1 && lesser == 2)
					{
						comboType = CardComboType::QUADRUPLE4;
						return;
					}
					if (findMaxSeq() == curr && lesser == curr * 2 &&
						packs.begin()->level <= MAX_STRAIGHT_LEVEL)
					{
						comboType = CardComboType::SSHUTTLE4;
						return;
					}
				}
			}
		}

		comboType = CardComboType::INVALID;
	}

	/**
	* 判断指定牌组能否大过当前牌组（这个函数不考虑过牌的情况！）
	*/
	bool canBeBeatenBy(const CardCombo &b) const
	{
		if (comboType == CardComboType::INVALID || b.comboType == CardComboType::INVALID)
			return false;
		if (b.comboType == CardComboType::ROCKET)
			return true;
		if (b.comboType == CardComboType::BOMB)
			switch (comboType)
			{
			case CardComboType::ROCKET:
				return false;
			case CardComboType::BOMB:
				return b.comboLevel > comboLevel;
			default:
				return true;
			}
		return b.comboType == comboType && b.cards.size() == cards.size() && b.comboLevel > comboLevel;
	}

	/**
	* 从指定手牌中寻找第一个能大过当前牌组的牌组
	* 如果随便出的话只出第一张
	* 如果不存在则返回一个PASS的牌组
	*/
	template <typename CARD_ITERATOR>
	CardCombo findFirstValid(CARD_ITERATOR begin, CARD_ITERATOR end) const
	{
		if (comboType == CardComboType::PASS) // 如果不需要大过谁，只需要随便出
		{
			CARD_ITERATOR second = begin;
			second++;
			return CardCombo(begin, second); // 那么就出第一张牌……
		}

		// 然后先看一下是不是火箭，是的话就过
		if (comboType == CardComboType::ROCKET)
			return CardCombo();

		// 现在打算从手牌中凑出同牌型的牌
		auto deck = vector<Card>(begin, end); // 手牌
		short counts[MAX_LEVEL + 1] = {};

		unsigned short kindCount = 0;

		// 先数一下手牌里每种牌有多少个
		for (Card c : deck)
			counts[card2level(c)]++;

		// 手牌如果不够用，直接不用凑了，看看能不能炸吧
		if (deck.size() < cards.size())
			goto failure;

		// 再数一下手牌里有多少种牌
		for (short c : counts)
			if (c)
				kindCount++;

		// 否则不断增大当前牌组的主牌，看看能不能找到匹配的牌组
		{
			// 开始增大主牌
			int mainPackCount = findMaxSeq();
			bool isSequential =
				comboType == CardComboType::STRAIGHT ||
				comboType == CardComboType::STRAIGHT2 ||
				comboType == CardComboType::PLANE ||
				comboType == CardComboType::PLANE1 ||
				comboType == CardComboType::PLANE2 ||
				comboType == CardComboType::SSHUTTLE ||
				comboType == CardComboType::SSHUTTLE2 ||
				comboType == CardComboType::SSHUTTLE4;
			for (Level i = 1;; i++) // 增大多少
			{
				for (int j = 0; j < mainPackCount; j++)
				{
					int level = packs[j].level + i;

					// 各种连续牌型的主牌不能到2，非连续牌型的主牌不能到小王，单张的主牌不能超过大王
					if ((comboType == CardComboType::SINGLE && level > MAX_LEVEL) ||
						(isSequential && level > MAX_STRAIGHT_LEVEL) ||
						(comboType != CardComboType::SINGLE && !isSequential && level >= level_joker))
						goto failure;

					// 如果手牌中这种牌不够，就不用继续增了
					if (counts[level] < packs[j].count)
						goto next;
				}

				{
					// 找到了合适的主牌，那么从牌呢？
					// 如果手牌的种类数不够，那从牌的种类数就不够，也不行
					if (kindCount < packs.size())
						continue;

					// 好终于可以了
					// 计算每种牌的要求数目吧
					short requiredCounts[MAX_LEVEL + 1] = {};
					for (int j = 0; j < mainPackCount; j++)
						requiredCounts[packs[j].level + i] = packs[j].count;
					for (unsigned j = mainPackCount; j < packs.size(); j++)
					{
						Level k;
						for (k = 0; k <= MAX_LEVEL; k++)
						{
							if (requiredCounts[k] || counts[k] < packs[j].count)
								continue;
							requiredCounts[k] = packs[j].count;
							break;
						}
						if (k == MAX_LEVEL + 1) // 如果是都不符合要求……就不行了
							goto next;
					}

					// 开始产生解
					vector<Card> solve;
					for (Card c : deck)
					{
						Level level = card2level(c);
						if (requiredCounts[level])
						{
							solve.push_back(c);
							requiredCounts[level]--;
						}
					}
					return CardCombo(solve.begin(), solve.end());
				}

			next:; // 再增大
			}
		}

	failure:
		// 实在找不到啊
		// 最后看一下能不能炸吧

		for (Level i = 0; i < level_joker; i++)
			if (counts[i] == 4 && (comboType != CardComboType::BOMB || i > packs[0].level)) // 如果对方是炸弹，能炸的过才行
			{
				// 还真可以啊……
				Card bomb[] = {Card(i * 4), Card(i * 4 + 1), Card(i * 4 + 2), Card(i * 4 + 3)};
				return CardCombo(bomb, bomb + 4);
			}

		// 有没有火箭？
		if (counts[level_joker] + counts[level_JOKER] == 2)
		{
			Card rocket[] = {card_joker, card_JOKER};
			return CardCombo(rocket, rocket + 2);
		}

		// ……
		return CardCombo();
	}
	
	/**
	* 从指定手牌中寻找所有能大过当前牌组的牌组，包括PASS
	*/
	template <typename CARD_ITERATOR>
	vector < CardCombo > findAllValid(CARD_ITERATOR begin, CARD_ITERATOR end, bool ignore_common=0, bool ignore_pass=0) const
	{
		vector < CardCombo > ret;
		
		auto deck = vector<Card>(begin, end); // 手牌
		short counts[MAX_LEVEL + 1] = {};
		
		for (Card c : deck){
			counts[card2level(c)]++;
		}
		
		if(!ignore_common){// 炸弹，火箭和PASS
		
			// 看一下能不能炸吧
			for (Level i = 0; i < level_joker; i++)
				if (counts[i] == 4 && comboType!= CardComboType::ROCKET && (comboType != CardComboType::BOMB || i > packs[0].level))
				{
					// 还真可以啊……
					Card bomb[] = {Card(i * 4), Card(i * 4 + 1), Card(i * 4 + 2), Card(i * 4 + 3)};
					ret.emplace_back(bomb, bomb + 4);
				}

			// 有没有火箭？
			if (counts[level_joker] + counts[level_JOKER] == 2)
			{
				Card rocket[] = {card_joker, card_JOKER};
				ret.emplace_back(rocket, rocket + 2);
			}
			
			// PASS
			if(!ignore_pass) ret.emplace_back();
		}
		
		if (comboType == CardComboType::ROCKET || comboType == CardComboType::BOMB)
			return ret;
		
		vector < vector < vector < int > > > types;
		int levLB=0;
		int minMainCnt=1;
		
		int maxMainLen;
		
		if (comboType != CardComboType::PASS){
			if (deck.size() < cards.size())
				return ret;
			
			int seqLen=findMaxSeq();
			minMainCnt=packs[0].count;
			
			types.resize(seqLen+1);
			types[seqLen].assign(1,{minMainCnt,
									(int)packs.size()-seqLen,
									packs.back().count});
			maxMainLen = (int)types.size()-1;
			
			levLB=packs[0].level+1;
		}
		else{
			types = vector < vector < vector < int > > >(cardComboInfo,
														 cardComboInfo+recMainPacks+1);
			maxMainLen = maxMainPacks;
		}

		// 现在打算从手牌中凑出同牌型的牌
		vector < Card > clsfy[MAX_LEVEL + 1];

		unsigned short kindCount = 0;

		// 先数一下手牌里每种牌有多少个
		for (Card c : deck){
			clsfy[card2level(c)].push_back(c);
		}

		// 再数一下手牌里有多少种牌
		for (short c : counts)
			if (c)
				kindCount++;
		
		static int avaiAds[MAX_LEVEL + 1];
		int avaiCnt=0;
		static int got[card_JOKER + 1];
		int gotCnt=0;
		for(Level mxLev=level_JOKER; mxLev>=levLB; --mxLev){ // 枚举最大主牌的level
		
			int mxCnt=counts[mxLev]; // 主牌重数的上限
			int mxLen=min(mxLev+1,maxMainLen); // 主牌类数的上限
			if(mxLev>MAX_STRAIGHT_LEVEL) mxLen=1;
			
			vector < Card > mainCards;
			
			for(Level mnLev=mxLev; mnLev >= mxLev-mxLen+1 && mxCnt >= minMainCnt; --mnLev){
				
				int curLen=mxLev-mnLev+1;
				
				mxCnt = min(mxCnt,(int)counts[mnLev]);
				
				gotCnt=0;
				
				for(vector < int > info : types[min(curLen,(int)types.size()-1)]){
					
					int mainCnt=info[0],adPacks=info[1],adCnt=info[2];
					
					if(mainCnt > mxCnt) break;
					if(adPacks+curLen > kindCount || curLen*mainCnt + adPacks*adCnt > (int)deck.size())
						continue;
					
					avaiCnt=0;
					if(adPacks){
						for(int i=0;i<mnLev;++i){
							if(counts[i]>=adCnt) avaiAds[avaiCnt++]=i;
						}
						for(int i=mxLev+1;i<=level_JOKER;++i){
							if(counts[i]>=adCnt) avaiAds[avaiCnt++]=i;
						}
					}
					
					if(gotCnt < mainCnt * curLen){
						int ld=gotCnt/curLen,
							rd=mainCnt;
						
						for(int i=mnLev;i<=mxLev;++i){
							for(int j=ld;j<rd;++j)
								got[gotCnt++]=clsfy[i][j];
						}
					}
					
					for(const vector < int > &vec : subsets[avaiCnt][adPacks]){
						
						int tmpGotCnt = gotCnt;
						for(int id : vec){
							int real_id = avaiAds[id];
							for(int j=0;j<adCnt;++j)
								got[tmpGotCnt++]=clsfy[real_id][j];
						}
						
						ret.emplace_back(got,got+tmpGotCnt);
						if(ret.back().comboType == CardComboType::INVALID)
							ret.pop_back();
					}
				}
			}
		}
		
		return ret;
	}
	
	unsigned int hashVal() const{
		unsigned int ret=0;
		for(int i=level_JOKER;i>=0;--i){
			ret = ret*4 + myCounts[i];
		}
		return ret;
	}
	
	CardCombo extractFrom(const vector < Card > &C) const{
		static short toRemove[MAX_LEVEL + 1];
		memcpy(toRemove,myCounts,sizeof(short)*(MAX_LEVEL + 1));
		vector < Card > ret;
		ret.reserve(cards.size());
		for(Card c:C){
			int d=card2level(c);
			if(toRemove[d]){
				--toRemove[d];
				ret.push_back(c);
			}
		}
		return CardCombo(ret.begin(),ret.end());
	}
	
	void removeFromStrict(vector < Card > &C) const{
		vector < Card > nC;
		nC.reserve(C.size());
		for(Card c:C){
			if(!(cardsMask&(1ull<<c)))
				nC.push_back(c);
		}
		#ifdef LOCAL_QTY
			assert((int)nC.size() == (int)C.size() - (int)cards.size());
		#endif
		C=nC;
	}
	
	void removeFrom(vector < Card > &C) const{
		static short toRemove[MAX_LEVEL + 1];
		memcpy(toRemove,myCounts,sizeof(short)*(MAX_LEVEL + 1));
		vector < Card > nC;
		nC.reserve(C.size());
		for(Card c:C){
			int d=card2level(c);
			(toRemove[d])?(--toRemove[d]):(nC.push_back(c),0);
		}
		C=nC;
	}

	void debugPrint() const
	{
#ifndef _BOTZONE_ONLINE
		std::cout << "[" << cardComboStrings[(int)comboType] << " x " << cards.size() << ", with level" << comboLevel << "]";
#endif
	}
};

string debugInfo;

/* 状态 */


// 我的牌有哪些
set<Card> myCards;

// 地主明示的牌有哪些
set<Card> landlordPublicCards;

// 大家从最开始到现在都出过什么
set < Card > allPlayedCards;
vector < vector < Card > > whatTheyPlayed[PLAYER_COUNT];
vector < pair < int , vector < Card > > > historyMoves; // 所有历史牌组，顺序放置

// 当前要出的牌需要大过谁
CardCombo lastValidCombo;
int lastValidCombo_who;

// 大家还剩多少牌
short cardRemaining[PLAYER_COUNT] = {17, 17, 17};

// 我是几号玩家（座位号）
int myPosition;

// 地主位置（座位号）
int landlordPosition = -1;

// 地主叫分
int landlordBid = -1;

// 阶段
Stage stage = Stage::BIDDING;

// 自己的第一回合收到的叫分决策
vector<int> bidInput;

namespace BotzoneIO
{
	using namespace std;
	void read()
	{
		// 读入输入（平台上的输入是单行）
		string line;
		getline(cin, line);
		Json::Value input;
		Json::Reader reader;
		reader.parse(line, input);

		// 首先处理第一回合，得知自己是谁、有哪些牌
		{
			auto firstRequest = input["requests"][0u]; // 下标需要是 unsigned，可以通过在数字后面加u来做到
			auto own = firstRequest["own"];
			
			for (unsigned i = 0; i < own.size(); i++)
				myCards.insert(own[i].asInt());
			if (firstRequest.isMember("bid") && !firstRequest["bid"].isNull()){
				// 如果还可以叫分，则记录叫分
				auto bidHistory = firstRequest["bid"];
				myPosition = bidHistory.size();
				for (unsigned i = 0; i < bidHistory.size(); i++)
					bidInput.push_back(bidHistory[i].asInt());
			}
			else{
				myPosition = firstRequest["pos"].asInt();
			}
		}

		// history里第一项（上上家）和第二项（上家）分别是谁的决策
		int whoInHistory[] = {(myPosition - 2 + PLAYER_COUNT) % PLAYER_COUNT, (myPosition - 1 + PLAYER_COUNT) % PLAYER_COUNT};

		int turn = input["requests"].size();
		for (int i = 0; i < turn; i++)
		{
			auto request = input["requests"][i];
			auto llpublic = request["publiccard"];
			bool isFirstRnd = !llpublic.isNull();
			if (isFirstRnd)
			{
				// 第一次得知公共牌、地主叫分和地主是谁
				landlordPosition = request["landlord"].asInt();
				landlordBid = request["finalbid"].asInt();
				myPosition = request["pos"].asInt();
				cardRemaining[landlordPosition] += llpublic.size();
				for (unsigned j = 0; j < llpublic.size(); j++)
				{
					landlordPublicCards.insert(llpublic[j].asInt());
					if (landlordPosition == myPosition)
						myCards.insert(llpublic[j].asInt());
				}
			}

			auto history = request["history"]; // 每个历史中有上家和上上家出的牌
			if (history.isNull())
				continue;
			stage = Stage::PLAYING;

			// 逐次恢复局面到当前
			int howManyPass = 0;
			for (int p = 0; p < 2; p++)
			{
				int player = whoInHistory[p];	// 是谁出的牌
				auto playerAction = history[p]; // 出的哪些牌
				vector<Card> playedCards;
				for (unsigned _ = 0; _ < playerAction.size(); _++) // 循环枚举这个人出的所有牌
				{
					int card = playerAction[_].asInt(); // 这里是出的一张牌
					playedCards.push_back(card);
					allPlayedCards.insert(card);
				}
				whatTheyPlayed[player].push_back(playedCards); // 记录这段历史
				cardRemaining[player] -= playerAction.size();
				
				if (playerAction.size() == 0)
					howManyPass++;
				else{
					lastValidCombo = CardCombo(playedCards.begin(), playedCards.end());
					lastValidCombo_who = player;
				}
				
				if(!isFirstRnd || howManyPass < p+1){
					historyMoves.emplace_back(player,playedCards);
				}
			}

			if (howManyPass == 2){
				lastValidCombo = CardCombo();
				lastValidCombo_who = myPosition;
			}

			if (i < turn - 1)
			{
				// 还要恢复自己曾经出过的牌
				auto playerAction = input["responses"][i]; // 出的哪些牌
				vector<Card> playedCards;
				for (unsigned _ = 0; _ < playerAction.size(); _++) // 循环枚举自己出的所有牌
				{
					int card = playerAction[_].asInt(); // 这里是自己出的一张牌
					myCards.erase(card);				// 从自己手牌中删掉
					playedCards.push_back(card);
					allPlayedCards.insert(card);
				}
				whatTheyPlayed[myPosition].push_back(playedCards); // 记录这段历史
				cardRemaining[myPosition] -= playerAction.size();
				
				historyMoves.emplace_back(myPosition,playedCards);
			}
		}
		
		#ifdef LOCAL_QTY
			if(stage == Stage::PLAYING){
				assert((int)allPlayedCards.size() + cardRemaining[0] + cardRemaining[1] + cardRemaining[2] == 54);
			
				if(!allPlayedCards.empty()){
					int historyCardCnt = 0;
					assert(historyMoves[0].first == landlordPosition);
					
					for(int i=0; i < (int)historyMoves.size(); ++i){
						if(i < (int)historyMoves.size()-1)
							assert(historyMoves[i+1].first == (historyMoves[i].first+1)%3);
						
						historyCardCnt += historyMoves[i].second.size();
					}
					
					assert(historyCardCnt == (int)allPlayedCards.size());
				}
			}
		#endif
	}

	/**
	* 输出叫分（0, 1, 2, 3 四种之一）
	*/
	void bid(int value)
	{
		Json::Value result;
		result["response"] = value;
		
		if(!debugInfo.empty())
			result["debug"] = debugInfo;

		Json::FastWriter writer;
		cout << writer.write(result) << endl;
	}

	/**
	* 输出打牌决策，begin是迭代器起点，end是迭代器终点
	* CARD_ITERATOR是Card（即short）类型的迭代器
	*/
	template <typename CARD_ITERATOR>
	void play(CARD_ITERATOR begin, CARD_ITERATOR end)
	{
		Json::Value result, response(Json::arrayValue);
		for (; begin != end; begin++)
			response.append(*begin);
		result["response"] = response;
		
		if(!debugInfo.empty())
			result["debug"] = debugInfo;

		Json::FastWriter writer;
		cout << writer.write(result) << endl;
	}
}

class State{
public:
	vector < Card > cards[3],unassigned;
	int curPlayer,actionCnt[3];
	int bidding[3],landlordPos;
	int baseScore,finScore[3];
	bool gameEnded,biddingEnded;
	
	pair < CardCombo , int > toBeat;
	
	State(): curPlayer(0), actionCnt{}, bidding{-1,-1,-1}, landlordPos(-1),
			 baseScore(0), finScore{}, gameEnded(0), biddingEnded(0) {}
	
	template < typename Arr >
	void randomComplete(Arr nCardsRem){
		unsigned long long notAvai=0;
		for(int i=0;i<3;++i){
			for(Card c : cards[i])
				notAvai |= 1ull<<c;
		}
		for(Card c : unassigned)
			notAvai |= 1ull<<c;
		
		vector < Card > avai;
		avai.reserve(card_JOKER+1);
		for(int i=0;i<=card_JOKER;++i){
			if(!(notAvai&(1ull<<i)))
				avai.push_back(i);
		}
		shuffle(avai.begin(),avai.end(),rng);
		
		for(int i=0;i<3;++i){
			int tar = nCardsRem[i] - (int)cards[i].size();
			#ifdef LOCAL_QTY
				assert(tar>=0);
			#endif
			cards[i].insert(cards[i].end(),avai.end()-tar,avai.end());
			avai.erase(avai.end()-tar,avai.end());
		}
		
		unassigned.insert(unassigned.end(),avai.begin(),avai.end());
	}
	
	vector < CardCombo > nextMoves() const{
		if(gameEnded) return vector < CardCombo >();
		
		if(!biddingEnded){
			int minBid = (toBeat.first.comboType==CardComboType::PASS ? 1 : (int)toBeat.first.cards[0]+1);
			vector < CardCombo > ret(1);
			for(int i=minBid;i<=3;++i){
				vector < Card > dummyCards = {Card(i)};
				ret.emplace_back(dummyCards.begin(),dummyCards.end());
			}
			return ret;
		}
		
		bool cantPass = !actionCnt[landlordPos] || toBeat.second==curPlayer;
		return toBeat.first.findAllValid(cards[curPlayer].begin(),cards[curPlayer].end(),0,cantPass);
	}
	
	CardCombo makeMove(CardCombo move,bool needExtract){
		#ifdef LOCAL_QTY
			assert(!gameEnded);
		#endif
		
		if(!biddingEnded){
			bidding[curPlayer] = (move.comboType==CardComboType::PASS ? 0 : move.cards[0]);
			
			if(curPlayer==2 || bidding[curPlayer]==3){
				
				if(bidding[curPlayer])
					toBeat = make_pair(move,curPlayer);
				
				biddingEnded = 1;
				
				landlordPos = (toBeat.first.comboType==CardComboType::PASS ? 0 : toBeat.second);
				
				baseScore = max(1,bidding[landlordPos]);
				
				curPlayer = landlordPos;
				
				cards[landlordPos].insert(cards[landlordPos].end(),unassigned.begin(),unassigned.end());
				unassigned.clear();
				
				toBeat = make_pair(CardCombo(),-1);
			}
			else{
				if(move.comboType!=CardComboType::PASS)
					toBeat = make_pair(move,curPlayer);
				
				++curPlayer;
			}
			
			return move;
		}
		
		if(move.comboType==CardComboType::PASS){
			curPlayer = (curPlayer + 1) % 3;
			if(toBeat.second == curPlayer)
				toBeat.first = CardCombo();
			
			return move;
		}
		
		if(move.comboType==CardComboType::BOMB ||
		   move.comboType==CardComboType::ROCKET)
			   baseScore *= 2;
		
		if(needExtract)
			move = move.extractFrom(cards[curPlayer]);
		
		++actionCnt[curPlayer];
		
		move.removeFromStrict(cards[curPlayer]);
		
		if(cards[curPlayer].empty()){
			gameEnded = 1;
			
			if(curPlayer==landlordPos && !actionCnt[(curPlayer+1)%3]
									  && !actionCnt[(curPlayer+2)%3])
				baseScore *= 2;
			
			if(curPlayer!=landlordPos && actionCnt[landlordPos]==1)
				baseScore *= 2;
			
			int landlordCoe = (curPlayer==landlordPos ? 1 : -1);
			finScore[landlordPos] += landlordCoe * 2 * baseScore;
			finScore[(landlordPos+1)%3] -= landlordCoe * baseScore;
			finScore[(landlordPos+2)%3] -= landlordCoe * baseScore;
		}
		
		toBeat = make_pair(move,curPlayer);
		
		curPlayer = (curPlayer + 1) % 3;
		
		return move;
	}
	
	float scoreEstimate[3],landlordWinProb;
	
	void estimate(){
		if(!biddingEnded){
			scoreEstimate[0] = scoreEstimate[1] = scoreEstimate[2] = 0;
			return;
		}
		
		if(gameEnded){
			for(int i=0;i<3;++i)
				scoreEstimate[i]=finScore[i];
			
			return;
		}
		
		float competency[3]={};
		static short allCounts[3][MAX_LEVEL + 1];
		
		for(int i=0;i<3;++i){
			short *counts = allCounts[i];
			memset(counts,0,sizeof(short)*(MAX_LEVEL+1));
			for(Card c : cards[i])
				++counts[card2level(c)];
			
			Level first;
			for(first=0; first<=level_JOKER && counts[first]!=1; ++first);
			
			int structScore = 0;
			for(Level mxLev=level_joker-1; mxLev>=0; --mxLev){
				int lowerBnd = max(0, mxLev-maxRectWidth+1);
				if(mxLev>MAX_STRAIGHT_LEVEL) lowerBnd = mxLev;
				int height = counts[mxLev];
				for(Level mnLev=mxLev;
					height && mnLev>=lowerBnd;
					--mnLev, height=min(height,(int)counts[mnLev])){
						structScore += rectScoresPresum[mxLev-mnLev][height-1];
				}
			}
			if(counts[level_joker] && counts[level_JOKER])
				structScore += 8;
			
			float levelScore = 0;
			for(Level j=level_JOKER; j>=0; --j){
				levelScore *= PARA::levelWeight_E;
				levelScore += counts[j] / ((j==first)+1.);
			}
			
			float playedScore = 0;
			if(i == curPlayer && toBeat.second == i){
				playedScore = PARA::myStt_D;
			}
			else if(toBeat.first.comboType != CardComboType::PASS && toBeat.second == i){
				int w = toBeat.first.findMaxSeq(),
					h = toBeat.first.packs[0].count,
					lev = toBeat.first.packs[0].level - w + 1;
				
				if(lev==level_joker && w==2){
					playedScore = 12;
				}
				else if(w>5){
					float levelWeight = pow(PARA::levelWeight_E,
											(float)lev / (playedScore_iE[4][h-1] + h*(w-5)));
					playedScore = maxPlayedScore_B * levelWeight;
				}
				else{
					float levelWeight = pow(PARA::levelWeight_E,
											(float)lev / playedScore_iE[w-1][h-1]);
					playedScore = playedScore_B[w-1][h-1] * levelWeight;
				}
				
				playedScore += PARA::myCrd_D;
			}
			
			competency[i] = levelScore
						  + PARA::structWeight * structScore
						  + PARA::playedWeight * playedScore;
			
			competency[i] /= (cards[i].size()-counts[first]/2.) * (cards[i].size()-counts[first]/2.);
		}
		
		float sumCompetency = competency[0] + competency[1] + competency[2];
		landlordWinProb = competency[landlordPos] / sumCompetency;
		
		scoreEstimate[landlordPos] = (2*landlordWinProb-1) * baseScore*2;
		scoreEstimate[(landlordPos+1)%3] = -scoreEstimate[landlordPos]/2;
		scoreEstimate[(landlordPos+2)%3] = -scoreEstimate[landlordPos]/2;
		
		int coe=1;
		
		for(int i=0;i<3;++i){
			if(scoreEstimate[i]<0) continue;
			short *count=allCounts[i];
			if(count[level_joker] && count[level_JOKER])
				coe*=2;
			
			for(int j=0;j<level_joker;++j){
				if(count[j]==4) coe*=2;
			}
		}
		
		if(coe>1){
			for(int i=0;i<3;++i)
				scoreEstimate[i]*=coe;
		}
	}
	
	ProbType estimateProb(const CardCombo &action) const{
		if(!biddingEnded) return 1;
		
		ProbType totWeight=0,targetWeight=0;
		bool foundMatch=0;
		
		vector < CardCombo > options = nextMoves();
		for(const CardCombo curAction : options){
			
			State tmpState = *this;
			tmpState.makeMove(curAction,0);
			tmpState.estimate();
			
			ProbType myWinProb = (curPlayer==landlordPos ? tmpState.landlordWinProb
														 : 1-tmpState.landlordWinProb);
			
			ProbType curWeight = pow(PARA::utilityToProb_E, myWinProb);
			totWeight += curWeight;
			if(curAction.hashVal() == action.hashVal()){
				targetWeight = curWeight;
				foundMatch = 1;
			}
		}
		
		if(!foundMatch) return 1;
		return targetWeight / totWeight;
	}
	
	float gameProgress() const{
		if(!biddingEnded) return 0;
		int curCnt = cards[0].size() + cards[1].size() + cards[2].size() + toBeat.first.cards.size();
		int totCnt = cardRemaining[0] + cardRemaining[1] + cardRemaining[2];
		return 1 - float(curCnt) / totCnt;
	}
};

#define nTotalEval_noheuris(x) ((x)?((x)->nEval):0)
#define exScore_noheuris(x,w,d) ((x)?((x)->evalSum[w]/(x)->nEval):(d))

const int POOL_SIZE=500000;

class MCTNode{
public:
	vector < pair < CardCombo , MCTNode * > > sons;
	int nEval,dep,curPlayer;
	float iniWeight[3],iniVal[3],evalSum[3];
	
	MCTNode(int _dep=0): nEval(0), dep(_dep), curPlayer(-1),
						 iniWeight{}, iniVal{}, evalSum{} {}
	
	float nTotalEval(int who) const{
		return iniWeight[who] + nEval;
	}
	
	float exScore(int who) const{
		if(iniWeight==0 && !nEval) return 0;
		return (iniVal[who]*iniWeight[who] + evalSum[who]) / nTotalEval(who);
	}
	
	bool assignState(State,bool,bool,float);
	
	int selectSon() const{
		#ifdef LOCAL_QTY
			assert(!sons.empty());
		#endif
		
		vector < int > per(sons.size());
		for(int i=0;i<(int)per.size();++i) per[i]=i;
		shuffle(per.begin(),per.end(),rng);
		
		if(PARA::useHeuris){
			float U = nEval+(int)sons.size();
			for(int i=0;i<(int)sons.size();++i)
				U += sons[per[i]].second->iniWeight[curPlayer];
			U = log(U);
			
			int selected = per[0];
			float val = sons[per[0]].second->exScore(curPlayer)
					  + PARA::c*sqrt(U/(sons[per[0]].second->nTotalEval(curPlayer)+1));
			
			for(int i=1;i<(int)sons.size();++i){
				MCTNode *curNode = sons[per[i]].second;
				float curVal = curNode->exScore(curPlayer)
							 + PARA::c*sqrt(U/(curNode->nTotalEval(curPlayer)+1));
				
				if(curVal>val){
					val = curVal;
					selected = per[i];
				}
			}
			
			return selected;
		}
		
		float dft = 0;
		if(!PARA::default0 && nEval){
			dft = evalSum[curPlayer] / nEval;
		}
		
		int selected = per[0];
		float U = log(nEval+(int)sons.size());
		float val = exScore_noheuris(sons[per[0]].second,curPlayer,dft)
				  + PARA::c*sqrt(U/(nTotalEval_noheuris(sons[per[0]].second)+1));
		
		for(int i=1;i<(int)sons.size();++i){
			MCTNode *curNode = sons[per[i]].second;
			float curVal = exScore_noheuris(curNode,curPlayer,dft)
						 + PARA::c*sqrt(U/(nTotalEval_noheuris(curNode)+1));
			
			if(curVal>val){
				val = curVal;
				selected = per[i];
			}
		}
		
		return selected;
	}
	
	void recordResult(const State &S){
		#ifdef LOCAL_QTY
			assert(S.gameEnded);
		#endif
		++nEval;
		for(int i=0;i<3;++i)
			evalSum[i]+=S.finScore[i];
	}
}pool[POOL_SIZE];
int poolPtr;

MCTNode *newnode(int dep=0){
	pool[poolPtr] = MCTNode(dep);
	return pool+(poolPtr++);
}

bool MCTNode::assignState(State S,bool rerunEstimate,bool extSons,float sVal){
	if(!sons.empty()) return 0;
	
	curPlayer = S.curPlayer;
	
	if(PARA::useHeuris){
		iniWeight[0] = iniWeight[1] = iniWeight[2]
					 = PARA::iniWeight_B * sVal * pow(PARA::iniWeight_E,S.gameProgress());
		
		if(rerunEstimate) S.estimate();
		memcpy(iniVal,S.scoreEstimate,sizeof(float)*3);
	}
	
	vector < CardCombo > moves = S.nextMoves();
	sons.resize(moves.size());
	for(int i=0;i<(int)sons.size();++i){
		sons[i] = make_pair(moves[i],(MCTNode*)0);
		
		if(extSons){
			State nS=S;
			nS.makeMove(moves[i],0);
			nS.estimate();
			
			MCTNode *ptr = (sons[i].second = newnode(dep+1));
			ptr->curPlayer = nS.curPlayer;
			
			memcpy(ptr->iniVal,nS.scoreEstimate,sizeof(float)*3);
			
			ptr->iniWeight[0] = ptr->iniWeight[1] = ptr->iniWeight[2]
							  = PARA::iniWeight_B * sVal * pow(PARA::iniWeight_E,nS.gameProgress());
		}
	}
	
	return 1;
}

ProbType getStateLogProb(State S){
	if(!S.biddingEnded || !PARA::useInference) return 0;
	
	for(const pair < int , vector < Card > > &move : historyMoves){
		S.cards[move.first].insert(S.cards[move.first].end(), move.second.begin(), move.second.end());
	}
	#ifdef LOCAL_QTY
		for(int i=0;i<3;++i){
			assert((int)S.cards[i].size() == (i==landlordPosition ? 20 : 17));
		}
	#endif
	
	ProbType res = 0;
	
	S.toBeat = make_pair(CardCombo(),-1);
	S.actionCnt[0] = S.actionCnt[1] = S.actionCnt[2] = 0;
	S.curPlayer = landlordPosition;
	S.gameEnded = 0;
	
	for(const pair < int , vector < Card > > &move : historyMoves){
		CardCombo curAction = CardCombo(move.second.begin(),move.second.end());
		#ifdef LOCAL_QTY
			assert(curAction.comboType != CardComboType::INVALID);
			assert(move.first == S.curPlayer);
		#endif
		res += log(S.estimateProb(curAction));
		S.makeMove(curAction,0);
	}
	
	return res;
}

constexpr int estimatedTotPlayouts = 2500;

CardCombo DetMCTS(State baseState,float rnd,float timeLeft){	
	float SoverD = PARA::SoverD_B * pow(PARA::SoverD_E,rnd);
	float startTime = (float)clock() / CLOCKS_PER_SEC;
	int DetDone = 0, totPlayouts = 0;
	
	const int PLAYOUTS_PER_BATCH = 10;
	
	vector < CardCombo > options = baseState.nextMoves();
	#ifdef LOCAL_QTY
		assert(!options.empty());
	#endif
	vector < long double > exScores(options.size());
	vector < unsigned int > hashVals(options.size());
	
	for(int i=0;i<(int)options.size();++i){
		exScores[i] = 0;
		hashVals[i] = options[i].hashVal();
	}
	
	const int TRIALS_PER_BATCH = 50, MAX_TRIALS = 10000;
	
	vector < pair < ProbType , State > > candidateStates;
	
	for(int i=0; i<MAX_TRIALS; i+=TRIALS_PER_BATCH){
		float takenTime = (float)clock() / CLOCKS_PER_SEC - startTime;
			if(takenTime >= timeLeft * PARA::inferenceTL)
				break;
		
		for(int j=0; j<TRIALS_PER_BATCH; ++j){
			State curState = baseState;
			curState.randomComplete(cardRemaining);
			ProbType curProb = getStateLogProb(curState);
			candidateStates.emplace_back(curProb,curState);
		}
	}
	
	vector < int > candidateOrd(candidateStates.size());
	for(int i=0; i<(int)candidateStates.size(); ++i){
		candidateOrd[i] = i;
	}
	sort(candidateOrd.begin(),candidateOrd.end(),[&](int a,int b){return candidateStates[a].first > candidateStates[b].first;});
	
	float curTime = (float)clock() / CLOCKS_PER_SEC;
	#ifdef LOCAL_QTY
		printf("inference took %.3f, did %d trials\n", curTime-startTime, (int)candidateStates.size());
	#endif
	timeLeft -= curTime - startTime;
	startTime = curTime;
	
	ProbType lastProb;
	
	for(int detID=0; /*detID<(int)candidateOrd.size()*/; ++detID){
		// perform one determinization
		State startingState;
		ProbType prob;
		if(detID<(int)candidateOrd.size()){
			tie(prob,startingState) = candidateStates[candidateOrd[detID]];
			lastProb = prob;
		}
		else{
			startingState = baseState;
			startingState.randomComplete(cardRemaining);
			prob = getStateLogProb(startingState);
		}
		#ifdef LOCAL_QTY
			if(detID%5 == 0) printf("%2d: %.10f\n",detID,prob);
		#endif
		
		poolPtr = 0;
		MCTNode *root = newnode();
		
		float estimatedPlayouts = sqrt(estimatedTotPlayouts * SoverD);
		if(DetDone) estimatedPlayouts = (float)totPlayouts / DetDone;
		
		// perform playouts
		bool isFirst=1;
		while(1){
			float takenTime = (float)clock() / CLOCKS_PER_SEC - startTime;
			if(takenTime >= timeLeft)
				break;
			
			float timePerDet = takenTime / (DetDone + 1),
				  PlayoutPerDet = (float)totPlayouts / (DetDone + 1),
				  nDets = timeLeft / timePerDet;
			if(!isFirst && PlayoutPerDet / nDets >= SoverD)
				break;
			
			isFirst=0;
			for(int i=0;i<PLAYOUTS_PER_BATCH;++i){
				++totPlayouts;
				
				State curState = startingState;
				MCTNode *curNode = root;
				
				vector < MCTNode * > stk{root};
				while(!curState.gameEnded){
					bool assigned = curNode->assignState(curState,1,PARA::useHeuris,estimatedPlayouts);
					
					int id = curNode->selectSon();
					MCTNode *&nx = curNode->sons[id].second;
					if(!nx) nx=newnode(curNode->dep+1);
					
					curState.makeMove(curNode->sons[id].first,!assigned);
					curNode=nx;
					stk.push_back(nx);
				}
				
				for(MCTNode *ptr : stk)
					ptr->recordResult(curState);
			}
		}
		
		#ifdef LOCAL_QTY
			assert(isFirst || !root->sons.empty());
		#endif
		
		float dft = 0;
		if(!PARA::default0 && root->nEval){
			dft = root->evalSum[root->curPlayer] / root->nEval;
		}
		
		for(int i=0;i<(int)root->sons.size();++i){
			#ifdef LOCAL_QTY
				assert(root->sons[i].first.hashVal() == hashVals[i]);
			#endif
			if(PARA::useHeuris)
				exScores[i] += root->sons[i].second->exScore(root->curPlayer) * expl(prob);
			else
				exScores[i] += exScore_noheuris(root->sons[i].second,root->curPlayer,dft) * expl(prob);
		}
		
		++DetDone;
		float takenTime = (float)clock() / CLOCKS_PER_SEC - startTime;
		if(takenTime >= timeLeft)
			break;
	}
	
	debugInfo = to_string(cardRemaining[0] + cardRemaining[1] + cardRemaining[2]) + " "
			  + to_string(DetDone) + " "
			  + to_string(totPlayouts) + " "
			  + to_string((int)candidateStates.size()) + " "
			  + to_string(lastProb);
	#ifdef LOCAL_QTY
		printf("%d %d\n",DetDone,totPlayouts);
	#endif
	
	return options[max_element(exScores.begin(),exScores.end()) - exScores.begin()];
}

int main()
{
	#ifdef LOCAL_QTY
		BotzoneIO::read();
		float baseTime = (float)clock() / CLOCKS_PER_SEC;
		assert((int)myCards.size()==cardRemaining[myPosition]);
	#else
		float baseTime = (float)clock() / CLOCKS_PER_SEC;
		BotzoneIO::read();
	#endif
	
	precSubsets();
	
	if (stage == Stage::BIDDING)
	{
		// 做出决策（你只需修改以下部分）
		
		State baseState;
		
		baseState.curPlayer = (int)bidInput.size();
		
		for(int i=0;i<(int)bidInput.size();++i)
			baseState.bidding[i] = bidInput[i];
		
		baseState.cards[baseState.curPlayer] = vector < Card >(myCards.begin(),myCards.end());
		
		if(bidInput.empty())
			baseState.toBeat = make_pair(CardCombo(),-1);
		else{
			auto maxIt = max_element(bidInput.begin(),bidInput.end());
			vector < Card > dummyCards = {Card(*maxIt)};
			baseState.toBeat = make_pair(CardCombo(dummyCards.begin(),dummyCards.end()),maxIt-bidInput.begin());
		}
		
		float curTime = (float)clock() / CLOCKS_PER_SEC;
		
		CardCombo res = DetMCTS(baseState,0,0.925-(curTime-baseTime));
		
		// 决策结束，输出结果（你只需修改以上部分）

		BotzoneIO::bid(res.comboType==CardComboType::PASS ? 0 : res.cards[0]);
	}
	else if (stage == Stage::PLAYING)
	{
		// 做出决策（你只需修改以下部分）
		
		State baseState;
		
		baseState.biddingEnded = 1;
		baseState.curPlayer = myPosition;
		
		for(int i=0;i<3;++i){
			for(vector < Card > vec : whatTheyPlayed[i]){
				baseState.unassigned.insert(baseState.unassigned.end(),vec.begin(),vec.end());
			}
		}
		
		baseState.cards[myPosition] = vector < Card >(myCards.begin(),myCards.end());
		
		for(Card c : landlordPublicCards){
			if(allPlayedCards.find(c) == allPlayedCards.end())
				baseState.cards[landlordPosition].push_back(c);
		}
		
		for(int i=0;i<3;++i){
			vector < Card > &vec=baseState.cards[i];
			sort(vec.begin(),vec.end());
			vec.erase(unique(vec.begin(),vec.end()), vec.end());
		}
		
		for(int i=0;i<3;++i){
			baseState.actionCnt[i] = (cardRemaining[i] < 17 + (i==landlordPosition ? 3 : 0));
		}
		
		baseState.landlordPos = landlordPosition;
		baseState.baseScore = landlordBid;
		baseState.toBeat = make_pair(lastValidCombo,lastValidCombo_who);
		
		float curTime = (float)clock() / CLOCKS_PER_SEC,
			  curRnd = 1 - float(cardRemaining[0] + cardRemaining[1] + cardRemaining[2]) / 54;
		
		CardCombo myAction = DetMCTS(baseState,curRnd,0.925-(curTime-baseTime));
		
		#ifdef LOCAL_QTY
			// 是合法牌
			assert(myAction.comboType != CardComboType::INVALID);

			assert(
				// 在上家没过牌的时候过牌
				(lastValidCombo.comboType != CardComboType::PASS && myAction.comboType == CardComboType::PASS) ||
				// 在上家没过牌的时候出打得过的牌
				(lastValidCombo.comboType != CardComboType::PASS && lastValidCombo.canBeBeatenBy(myAction)) ||
				// 在上家过牌的时候出合法牌
				(lastValidCombo.comboType == CardComboType::PASS && myAction.comboType != CardComboType::INVALID));
		#endif
		
		// 决策结束，输出结果（你只需修改以上部分）

		BotzoneIO::play(myAction.cards.begin(), myAction.cards.end());
	}
	
	return 0;
}