#include <iostream>
#include <set>
#include <string>
#include <cassert>
#include <cstring> // 注意memset是cstring里的
#include <algorithm>
#include <random>
#include <cmath>
#include <ctime>
#include <tuple>
#include <queue>
#include <fstream>
#include "jsoncpp/json.h" // 在平台上，C++编译时默认包含此库

// #define DEBUGGING

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
using std::ifstream;
using std::ofstream;
using std::ios;
using std::stoll;
using std::getline;
using std::endl;

const string chars = "34567890JQKA2rR";

constexpr int PLAYER_COUNT = 3;

mt19937 rng;

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
		std::cout << "[" << cardComboStrings[(int)comboType] << " x " << cards.size() << ", with level" << comboLevel << "] ";
		vector < Card > tmp = cards;
		sort(tmp.begin(),tmp.end());
		for(Card c : tmp){
			putchar(chars[card2level(c)]);
		}
		putchar('\n');
#endif
	}
};

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

	void printState(){
		printf("base score %d\n",baseScore);
		for(int i=0;i<3;++i){
			printf("player %d: ",i);
			if(i==landlordPos) printf("(landlord) ");
			if(i==curPlayer) printf("(cur) ");
			printf("\n");
			vector < Card > tmp=cards[i];
			sort(tmp.begin(),tmp.end());
			for(Card c : tmp) putchar(chars[card2level(c)]);
			printf("\n");
		}
		printf("\n");
	}
};

ifstream gameParticipants("game-participants.txt",ios::in);
ofstream gameScores("game-scores.txt",ios::out);
ofstream gameLog("game-log.txt",ios::out);

string commands[3];

Json::Value sentInfo[3];

int main(int argv,char **argc){
	assert(argv);
	long long seed = stoll(string(argc[1]));
	rng = mt19937(seed);

	for(int i=0;i<3;++i){
		getline(gameParticipants,commands[i]);
		while(commands[i].back()=='\n' || commands[i].back()=='\r')
			commands[i].pop_back();
		commands[i] += " >bot-out.txt <bot-in.txt";
		commands[i] = "./" + commands[i];
		gameLog << commands[i] << endl;
	}

	State game;
	game.randomComplete(vector < int >{17,17,17});
	vector < Card > pubcards = game.unassigned;

	for(int i=0;i<3;++i){
		sentInfo[i]["requests"].resize(0);
		sentInfo[i]["responses"].resize(0);
	}

	{// 叫分阶段
		for(int i=0;i<3 && !game.biddingEnded;++i){
			Json::Value curReq;
			for(Card c : game.cards[i])
				curReq["own"].append(c);
			
			curReq["bid"].resize(0);
			for(int j=0;j<i;++j){
				curReq["bid"].append(game.bidding[j]);
			}

			sentInfo[i]["requests"].append(curReq);
			
			Json::FastWriter writer;
			ofstream botIn("bot-in.txt",ios::out);
			string str = writer.write(sentInfo[i]);
			while(str.back()=='\n' || str.back()=='\r') str.pop_back();
			botIn << str << endl;
			botIn.close();
			gameLog << "\nJudge: " << str << endl;

			system(commands[i].c_str());

			ifstream botOut("bot-out.txt",ios::in);
			string line;
			getline(botOut, line);
			gameLog << "Bot " << i << ": " << line << endl;
			Json::Value response;
			Json::Reader reader;
			reader.parse(line, response);
			response = response["response"];
			botOut.close();
			sentInfo[i]["responses"].append(response);

			vector < Card > playedNow{short(response.asInt())};
			if(playedNow.empty() || playedNow[0]==0){
				game.makeMove(CardCombo(),0);
				#ifdef DEBUGGING
					printf("Player %d bid %d\n",i,0);
				#endif
			}
			else{
				game.makeMove(CardCombo(playedNow.begin(),playedNow.end()),0);
				#ifdef DEBUGGING
					printf("Player %d bid %d\n",i,(int)playedNow[0]);
				#endif
			}
		}
	}

	{// 出牌阶段
		bool told[3]={};
		vector < vector < Card > > played;
		while(!game.gameEnded){
			#ifdef DEBUGGING
				game.printState();
			#endif
			int cur = game.curPlayer;

			Json::Value curReq;
			unsigned long long pubMask=0;
			if(!told[cur]){
				for(Card c : pubcards){
					curReq["publiccard"].append(c);
					pubMask |= 1ull << c;
				}
				for(Card c : game.cards[cur]){
					if(!(pubMask & (1ull << c)))
						curReq["own"].append(c);
				}
				curReq["landlord"] = game.landlordPos;
				curReq["pos"] = cur;
				curReq["finalbid"] = game.baseScore;
				told[cur] = 1;
			}

			for(int i=0;i<2;++i){
				int id = (int)played.size()-(2-i);
				Json::Value cardList;
				cardList.resize(0);
				if(id>=0){
					for(Card c : played[id])
						cardList.append(c);
				}
				curReq["history"].append(cardList);
			}

			sentInfo[cur]["requests"].append(curReq);
			
			Json::FastWriter writer;
			ofstream botIn("bot-in.txt",ios::out);
			string str = writer.write(sentInfo[cur]);
			while(str.back()=='\n' || str.back()=='\r') str.pop_back();
			botIn << str << endl;
			botIn.close();
			gameLog << "\nJudge: " << str << endl;

			system(commands[cur].c_str());

			ifstream botOut("bot-out.txt",ios::in);
			string line;
			getline(botOut, line);
			gameLog << "Bot " << cur << ": " << line << endl;
			Json::Value response;
			Json::Reader reader;
			reader.parse(line, response);
			response = response["response"];
			botOut.close();
			sentInfo[cur]["responses"].append(response);

			vector < Card > playedNow;
			for(int i=0;i<(int)response.size();++i){
				playedNow.push_back(response[i].asInt());
			}
			game.makeMove(CardCombo(playedNow.begin(),playedNow.end()),0);
			played.push_back(playedNow);
			#ifdef DEBUGGING
				printf("Player %d: ",cur);
				CardCombo(playedNow.begin(),playedNow.end()).debugPrint();
				printf("\n\n");
			#endif
		}
	}

	for(int i=0;i<3;++i){
		gameScores << game.finScore[i] << endl;
		#ifdef DEBUGGING
			printf("Played %d: %d pt\n",i,game.finScore[i]);
		#endif
	}
	return 0;
}