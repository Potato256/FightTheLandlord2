#include <iostream>
#include <set>
#include <string>
#include <cassert>
#include <cstring> // 注意memset是cstring里的
#include <algorithm>
#include<vector>
#include <time.h> 
#include <cstdlib> 
#include "jsoncpp/json.h" 
using std::set;
using std::sort;
using std::string;
using std::unique;
using std::vector;
constexpr int PLAYER_COUNT = 3;
using namespace std;
clock_t start;
clock_t end_of_time;
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
int cardComboScores[] = {
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
	"INVALID" };
#endif

#define Test_Switch
// 用0~53这54个整数表示唯一的一张牌
using Card = short;
constexpr Card card_joker = 52;
constexpr Card card_JOKER = 53;
// 除了用0~53这54个整数表示唯一的牌，
// 这里还用另一种序号表示牌的大小（不管花色），以便比较，称作等级（Level）
// 对应关系如下：
// 3 4 5 6 7 8 9 10	J Q K	 A	2	 小王	 大王
// 0 1 2 3 4 5 6 7	8 9 10 11	12 13	  14
using Level = short;
constexpr Level MAX_LEVEL = 15;
constexpr Level MAX_STRAIGHT_LEVEL = 11;
constexpr Level level_joker = 13;
constexpr Level level_JOKER = 14;
/* 将Card变成Level*/
constexpr Level card2level(Card card)
{
	return card / 4 + card / 53;
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

		bool operator<(const CardPack& b) const
		{
			if (count == b.count)
				return level > b.level;
			return count > b.count;
		}
	};
	vector<Card> cards;		 // 原始的牌，未排序
	vector<CardPack> packs;	 // 按数目和大小排序的牌种
	CardComboType comboType; // 算出的牌型
	Level comboLevel = 0;	 // 算出的大小序
	int seq = 1;
	/**
						  * 检查个数最多的CardPack递减了几个
						  */
	int findMaxSeq() const
	{
		if (packs.empty())
			return 0;
		if (packs.size() == 1)
			return 1;
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
	CardCombo() : comboType(CardComboType::PASS) {}

	/**
	* 通过Card（即short）类型的迭代器创建一个牌型
	* 并计算出牌型和大小序等
	* 假设输入没有重复数字（即重复的Card）
	*/

	friend bool operator<(const CardCombo& c1, const CardCombo& c2)
		// 用来构造 set
	{
		if (c1.comboType == c2.comboType)
		{
			if (c1.comboLevel == c2.comboLevel)
				if (c1.packs.size() == c2.packs.size())
				{
					if (c1.comboType != CardComboType::TRIPLET1 &&
						c1.comboType != CardComboType::TRIPLET2 &&
						c1.comboType != CardComboType::QUADRUPLE2 &&
						c1.comboType != CardComboType::PLANE1 &&
						c1.comboType != CardComboType::PLANE2 &&
						c1.comboType != CardComboType::QUADRUPLE4)
						return 0;
					int n = c1.packs.size();
					for (int i = 0; i < n; ++i)
					{
						if (c1.packs[i].level == c2.packs[i].level)
							continue;
						else
							return c1.packs[i].level < c2.packs[i].level;
					}
				}
				else
					return c1.packs.size() < c2.packs.size();
			else
				return c1.comboLevel < c2.comboLevel;
		}
		else
			return c1.comboType < c2.comboType;
		return 0;
	}
	//-------------------------------------------------------------------
	//------------------------lbx's constructor--------------------------
	//-------------------------------------------------------------------
	CardCombo(CardComboType myType, Level l1 = 0, int len = 1, Level* l2 = 0)
		//  l1 是组合里牌首先最多，其次最大的类型
		//  l2 指向那些被带着的散牌（三带，四带，飞机） 
	{
		comboType = myType;
		comboLevel = l1;
		seq = len;
		switch (myType)
		{
		case CardComboType::ROCKET:
			packs.push_back(CardPack{ (Level)14, 1 });
			packs.push_back(CardPack{ (Level)13, 1 });
			break;
		case CardComboType::SINGLE:
			packs.push_back(CardPack{ l1, 1 });
			break;
		case CardComboType::PAIR:
			packs.push_back(CardPack{ l1, 2 });
			break;
		case CardComboType::BOMB:
			packs.push_back(CardPack{ l1, 4 });
			break;
		case CardComboType::PLANE:
			for (Level i = 0; i < seq; ++i)
				packs.push_back(CardPack{ (l1 - i), 3 });
			break;
		case CardComboType::PLANE1:
			for (Level i = 0; i < seq; ++i)
				packs.push_back(CardPack{ l1 - i, 3 });
			for (int i = 0; i < seq; ++i)
				packs.push_back(CardPack{ l2[i + 1], 1 });
			break;
		case CardComboType::PLANE2:
			for (Level i = 0; i < seq; ++i)
				packs.push_back(CardPack{ l1 - i, 3 });
			for (int i = 0; i < seq; ++i)
				packs.push_back(CardPack{ l2[i + 1], 2 });
			break;
		case CardComboType::QUADRUPLE2:
			packs.push_back(CardPack{ l1, 4 });
			packs.push_back(CardPack{ l2[0 + 1], 1 });
			packs.push_back(CardPack{ l2[1 + 1], 1 });
			break;
		case CardComboType::QUADRUPLE4:
			packs.push_back(CardPack{ l1, 4 });
			packs.push_back(CardPack{ l2[0 + 1], 2 });
			packs.push_back(CardPack{ l2[1 + 1], 2 });
			break;
		case CardComboType::STRAIGHT:
			for (Level i = 0; i < seq; ++i)
				packs.push_back(CardPack{ l1 - i, 1 });
			break;
		case CardComboType::STRAIGHT2:
			for (Level i = 0; i < seq; ++i)
				packs.push_back(CardPack{ l1 - i, 2 });
			break;
		case CardComboType::TRIPLET:
			packs.push_back(CardPack{ l1 , 3 });
			break;
		case CardComboType::TRIPLET1:
			packs.push_back(CardPack{ l1 , 3 });
			packs.push_back(CardPack{ l2[0 + 1],1 });
			break;
		case CardComboType::TRIPLET2:
			packs.push_back(CardPack{ l1 , 3 });
			packs.push_back(CardPack{ l2[0 + 1],2 });
			break;
		}
	}
	//-----------------------------------------------------------------------------
	template <typename CARD_ITERATOR>
	CardCombo(CARD_ITERATOR begin, CARD_ITERATOR end)
	{
		// 特判：空
		if (begin == end)
		{
			comboType = CardComboType::PASS;
			return;
		}

		// 每种牌有多少个
		short counts[MAX_LEVEL + 1] = {};

		// 同种牌的张数（有多少个单张、对子、三条、四条）
		short countOfCount[5] = {};

		cards = vector<Card>(begin, end);
		for (Card c : cards)
			counts[card2level(c)]++;
		for (Level l = 0; l <= MAX_LEVEL; l++)
			if (counts[l])
			{
				packs.push_back(CardPack{ l, counts[l] });
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
		sort(kindOfCountOfCount.begin(), kindOfCountOfCount.end());

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
		seq = findMaxSeq();
		comboType = CardComboType::INVALID;
	}

	/**
	* 判断指定牌组能否大过当前牌组（这个函数不考虑过牌的情况！）
	*/
	bool canBeBeatenBy(const CardCombo& b) const
	{
		if (comboType == CardComboType::INVALID || b.comboType == CardComboType::INVALID)
			return false;
		if (comboType == CardComboType::PASS && b.comboType != CardComboType::PASS)
			return true;
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
		return b.comboType == comboType && b.packs.size() == packs.size() && b.comboLevel > comboLevel;
	}

	/**
	* 从指定手牌中寻找第一个能大过当前牌组的牌组
	* 如果随便出的话只出第一张
	* 如果不存在则返回一个PASS的牌组
	*/
	CardCombo findValid(int hand_deck[][15], int My_position, int this_combo_from);
	//My_position是0代表地主,1代表农民甲,2代表农民乙,
	//hand_deck[0][]代表地主的手牌,hand_deck[1][]代表农民甲的手牌,hand_deck[2][]代表农民乙的手牌
	//hand_deck[0][0]代表地主手中3的数量,其他同理。


};

// 我的牌有哪些
set<Card> myCards;

// 地主明示的牌有哪些
set<Card> landlordPublicCards;

// 大家从最开始到现在都出过什么
vector<vector<Card>> whatTheyPlayed[PLAYER_COUNT];

// 当前要出的牌需要大过谁
CardCombo lastValidCombo;

// 大家还剩多少牌
short cardRemaining[PLAYER_COUNT] = { 17, 17, 17 };

// 我是几号玩家（0-地主，1-农民甲，2-农民乙）
int myPosition;

// 地主位置
int landlordPosition = -1;

// 地主叫分
int landlordBid = -1;

// 阶段
Stage stage = Stage::BIDDING;

// 自己的第一回合收到的叫分决策
vector<int> bidInput;
int grade[200] = { 0 };
int temp_hand_deck[3][15] = { 0 };
int imagined_hand_deck[3][15] = { 0 };

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
			if (!firstRequest["bid"].isNull())
			{
				// 如果还可以叫分，则记录叫分
				auto bidHistory = firstRequest["bid"];
				myPosition = bidHistory.size();
				for (unsigned i = 0; i < bidHistory.size(); i++)
					bidInput.push_back(bidHistory[i].asInt());
			}
		}
		// history里第一项（上上家）和第二项（上家）分别是谁的决策
		int whoInHistory[] = { (myPosition - 2 + PLAYER_COUNT) % PLAYER_COUNT, (myPosition - 1 + PLAYER_COUNT) % PLAYER_COUNT };


		int turn = input["requests"].size();
		for (int i = 0; i < turn; i++)
		{
			auto request = input["requests"][i];
			auto llpublic = request["publiccard"];
			if (!llpublic.isNull())
			{
				// 第一次得知公共牌、地主叫分和地主是谁
				landlordPosition = request["landlord"].asInt();
				landlordBid = request["finalbid"].asInt();
				myPosition = request["pos"].asInt();
				whoInHistory[0] = (myPosition - 2 + PLAYER_COUNT) % PLAYER_COUNT;
				whoInHistory[1] = (myPosition - 1 + PLAYER_COUNT) % PLAYER_COUNT;
				myPosition = ((myPosition - landlordPosition) + 3) % 3;
				whoInHistory[0] = ((whoInHistory[0] - landlordPosition) + 3) % 3;
				whoInHistory[1] = ((whoInHistory[1] - landlordPosition) + 3) % 3;
				landlordPosition = 0;
				cardRemaining[landlordPosition] += llpublic.size();
				for (unsigned i = 0; i < llpublic.size(); i++)
				{
					landlordPublicCards.insert(llpublic[i].asInt());
					if (landlordPosition == myPosition)
						myCards.insert(llpublic[i].asInt());
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
				}
				whatTheyPlayed[player].push_back(playedCards); // 记录这段历史
				cardRemaining[player] -= playerAction.size();

				if (playerAction.size() == 0)
					howManyPass++;
				else
					lastValidCombo = CardCombo(playedCards.begin(), playedCards.end());
			}

			if (howManyPass == 2)
				lastValidCombo = CardCombo();

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
				}
				whatTheyPlayed[myPosition].push_back(playedCards); // 记录这段历史
				cardRemaining[myPosition] -= playerAction.size();
			}
		}
	}

	/**
	* 输出叫分（0, 1, 2, 3 四种之一）
	*/
	void bid(int value)
	{
		Json::Value result;
		result["response"] = value;

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
		Json::Value debug(Json::arrayValue);
		for (; begin != end; begin++) {

			response.append(*begin);
			debug.append(*begin);
		}
		result["response"] = response;
		result["debug"] = debug;
		Json::FastWriter writer;
		cout << writer.write(result) << endl;
	}
}
void Guess_Others_Hand_Deck(int hand_deck[][15], int Card_num[], int My_position, int Card_to_Allocate[]) {
	int pos1 = (My_position + 1) % 3;
	int pos2 = (My_position + 2) % 3;
	int num1 = Card_num[pos1];
	int num2 = Card_num[pos2];
	int num3 = num1 + num2;
	vector<int> v;
	for (int i = 0; i < 15; ++i)
		for (int j = 0; j < Card_to_Allocate[i]; ++j)
			v.push_back(i);
	//std::cout << v.size() <<"  "<<num3 << "\n";
	random_shuffle(v.begin(), v.end());
	for (int i = 0; i < num1; ++i)
		++hand_deck[pos1][v[i]];
	for (int i = num1; i < num3; ++i)
		++hand_deck[pos2][v[i]];
	return;
}
const int features = 2;
class my_node
{
public:
	my_node* parent;
	CardCombo* c;
	int L;
	bool use;
	my_node(CardCombo* _c = 0, int l = 0) :L(l), c(_c), use(0) {}
};
int My_Bid(set<CardCombo>& best_Combos, int Last_Bid, int L_min)
{
	if (Last_Bid == 3)
		return 0;
	if (L_min <= 5)
		return 3;
	else if (L_min >= 7)
		return 0;
	for (auto i : best_Combos)
	{
		if (i.comboType == CardComboType::BOMB)
			return 3;
		if (i.comboType == CardComboType::SINGLE &&
			i.comboLevel == 13 || i.comboLevel == 14)
			return 3;
	}
	return 0;
}
int My_Bid_Brave(set<CardCombo>& best_Combos, int Last_Bid, int L_min,int hand_deck[])
{
	if (Last_Bid == 3)
		return 0;
	if (L_min <= 4)
		return 3;
	else if (L_min >= 8)
		return 0;
	bool bomb=0;
	for(int i=0;i<15;++i)
		if(hand_deck[i]==4)
			bomb=1;
	if(hand_deck[13]||hand_deck[14]||bomb)
		return 3;
	if(hand_deck[12]==0)
		return 0;
	if(hand_deck[12]>0&&L_min==5)
		return 3;
	if(hand_deck[12]+hand_deck[11]>3&&L_min==6)
		return 3;
	if(hand_deck[12]+hand_deck[11]>4&&L_min==7)
		return 3;
	return 0;
}
bool Check_Hand_Empty(int hand_deck[])
{
	for (int i = 0; i < 15; ++i)
		if (hand_deck[i])
			return 0;
	return 1;
}
bool Find_Single_Seq(int pos, int hand_deck[])
{
	int pos1 = std::max(pos - 4, 0);
	int pos2 = std::min(pos + 4, 11);
	int count = 0;
	for (int i = pos1; i < pos2; ++i)
	{
		if (hand_deck[i] < 1)
		{
			count = 0;
		}
		++count;
		if (count >= 5)
			return 1;
	}
	return 0;
}
bool Find_Pair_Seq(int pos, int hand_deck[])
{
	int pos1 = std::max(pos - 2, 0);
	int pos2 = std::min(pos + 2, 11);
	int count = 0;
	for (int i = pos1; i < pos2; ++i)
	{
		if (hand_deck[i] < 2)
		{
			count = 0;
		}
		++count;
		if (count >= 3)
			return 1;
	}
	return 0;
}
void Update_Hand_Deck(CardCombo& c, int hand_deck[])
{
	//cout << cardComboStrings[(int)c.comboType] << endl;
	for (auto i : c.packs)
	{
	//	cout << i.level <<" "<<i.count<< endl;
		hand_deck[i.level] -= i.count;

	}
	return;
}
void Search_Best_Combos(int hand_deck[], my_node& node, vector<my_node*>& node_vec, int& L_min)
{
	//std::cout << node.L << " " << L_min << "\n";
	//my_print(hand_deck, 15);
	//system("pause");
	bool find = 0;
	int hand_copy[15];
	if (node.L > L_min + features)
		return;
	if (Check_Hand_Empty(hand_deck))
	{
		L_min = std::min(L_min, node.L);
		node_vec.push_back(&node);
		//std::cout << "end " << L_min << "\n";
		return;
	}
	// 炸弹
	for (int i = 0; i < 13; ++i)
	{
		if (hand_deck[i] == 4)
		{
			// BOMB
			CardCombo* pc = new CardCombo(CardComboType::BOMB, i);
			my_node* pn = new my_node(pc, node.L + 1);
			pn->parent = &node;
			memcpy(hand_copy, hand_deck, 60);
			hand_copy[i] = 0;
			find = 1;
			Search_Best_Combos(hand_copy, *pn, node_vec, L_min);
		}
	}
	// 顺子
	for (int i = 0; i < 8; ++i)
	{
		bool flag = 1;
		for (int j = 0; j < 5; ++j)
			if (hand_deck[i + j] == 0)
				flag = 0;
		if (flag)
		{
			// STRAIGHT
			CardCombo* pc = new CardCombo(CardComboType::STRAIGHT, i + 4, 5);
			my_node* pn = new my_node(pc, node.L + 1);
			pn->parent = &node;
			memcpy(hand_copy, hand_deck, 60);
			Update_Hand_Deck(*pc, hand_copy);
			find = 1;
			Search_Best_Combos(hand_copy, *pn, node_vec, L_min);

			int j = 5;
			while ((i + j < 12) && hand_deck[i + j] > 0)
			{
				// LONGER STRAIGHT
				CardCombo* pc = new CardCombo(CardComboType::STRAIGHT, i + j, j + 1);
				my_node* pn = new my_node(pc, node.L + 1);
				pn->parent = &node;
				memcpy(hand_copy, hand_deck, 60);
				Update_Hand_Deck(*pc, hand_copy);
				find = 1;
				Search_Best_Combos(hand_copy, *pn, node_vec, L_min);
				++j;
			}
		}
	}
	// 连对
	for (int i = 0; i < 10; ++i)
	{
		bool flag = 1;
		for (int j = 0; j < 3; ++j)
			if (hand_deck[i + j] < 2)
				flag = 0;
		if (flag)
		{
			// STRAIGHT2
			CardCombo* pc = new CardCombo(CardComboType::STRAIGHT2, i + 2, 3);
			my_node* pn = new my_node(pc, node.L + 1);
			pn->parent = &node;
			memcpy(hand_copy, hand_deck, 60);
			Update_Hand_Deck(*pc, hand_copy);
			find = 1;
			Search_Best_Combos(hand_copy, *pn, node_vec, L_min);

			int j = 3;
			while ((i + j < 12) && hand_deck[i + j] >= 2)
			{
				// LONGER STRAIGHT2
				CardCombo* pc = new CardCombo(CardComboType::STRAIGHT2, i + j, j + 1);
				my_node* pn = new my_node(pc, node.L + 1);
				pn->parent = &node;
				memcpy(hand_copy, hand_deck, 60);
				Update_Hand_Deck(*pc, hand_copy);
				find = 1;
				Search_Best_Combos(hand_copy, *pn, node_vec, L_min);
				++j;
			}
		}
	}
	// 飞机1
	for (int i = 0; i < 11; ++i)
	{
		bool flag = 1;
		for (int j = 0; j < 2; ++j)
			if (hand_deck[i + j] < 3)
				flag = 0;
		if (flag)
		{
			// PLANE
			CardCombo* pc = new CardCombo(CardComboType::PLANE, i + 1, 2);
			my_node* pn = new my_node(pc, node.L + 1);
			pn->parent = &node;
			int hand_copy[15];
			memcpy(hand_copy, hand_deck, 60);
			Update_Hand_Deck(*pc, hand_copy);
			find = 1;
			Search_Best_Combos(hand_copy, *pn, node_vec, L_min); find = 1;

			int j = 2;
			while ((i + j < 12) && hand_deck[i + j] >= 3)
			{
				// LONGER PLANE
				CardCombo* pc = new CardCombo(CardComboType::STRAIGHT2, i + j, j + 1);
				my_node* pn = new my_node(pc, node.L + 1);
				pn->parent = &node;
				memcpy(hand_copy, hand_deck, 60);
				Update_Hand_Deck(*pc, hand_copy);
				find = 1;
				Search_Best_Combos(hand_copy, *pn, node_vec, L_min);
				++j;
			}
		}
	}
	// 三带二
	for (int i = 0; i < 13; ++i)
	{
		if (hand_deck[i] < 3)
			continue;
		for (int j = 0; j < 13; ++j)
		{
			if (j == i)
				continue;
			if (hand_deck[j] < 2)
				continue;
			// TRIPLET2
			Level* pl = new Level[2];
			pl[1] = j;
			CardCombo* pc = new CardCombo(CardComboType::TRIPLET2, i, 1, pl);
			my_node* pn = new my_node(pc, node.L + 1);
			pn->parent = &node;
			memcpy(hand_copy, hand_deck, 60);
			hand_copy[i] -= 3;
			hand_copy[j] -= 2;
			find = 1;
			Search_Best_Combos(hand_copy, *pn, node_vec, L_min);
		}
	}
	// 三带一
	for (int i = 0; i < 13; ++i)
	{
		if (hand_deck[i] < 3)
			continue;
		for (int j = 0; j < 13; ++j)
		{
			if (j == i)
				continue;
			if (hand_deck[j] < 1)
				continue;
			if (Find_Single_Seq(j, hand_deck))
				continue;
			// TRIPLET1
			Level* pl = new Level[2];
			pl[1] = j;
			CardCombo* pc = new CardCombo(CardComboType::TRIPLET1, i, 1, pl);
			my_node* pn = new my_node(pc, node.L + 1);
			pn->parent = &node;
			memcpy(hand_copy, hand_deck, 60);
			hand_copy[i] -= 3;
			hand_copy[j] -= 1;
			find = 1;
			Search_Best_Combos(hand_copy, *pn, node_vec, L_min);
		}
	}
	// 三条
	for (int i = 0; i < 13; ++i)
	{
		if (hand_deck[i] >= 3)
		{
			// TRIPLET
			CardCombo* pc = new CardCombo(CardComboType::TRIPLET, i);
			my_node* pn = new my_node(pc, node.L + 1);
			pn->parent = &node;
			memcpy(hand_copy, hand_deck, 60);
			hand_copy[i] -= 3;
			Search_Best_Combos(hand_copy, *pn, node_vec, L_min); find = 1;
		}
	}
	// 四带二对
	for (int i = 0; i < 13; ++i)
	{
		if (hand_deck[i] == 4)
		{
			for (int j = 0; j < 13; ++j)
			{
				if (j == i)
					continue;
				if (hand_deck[j] < 2)
					continue;
				for (int k = j + 1; k < 13; ++k)
				{
					if (k == i)
						continue;
					if (hand_deck[k] < 2)
						continue;
					// QUADRUPLE4
					Level* pl = new Level[3];
					pl[1] = j;
					pl[2] = k;
					CardCombo* pc = new CardCombo(CardComboType::QUADRUPLE4, i, 1, pl);
					my_node* pn = new my_node(pc, node.L + 1);
					pn->parent = &node;
					memcpy(hand_copy, hand_deck, 60);
					hand_copy[i] = 0;
					hand_copy[j] -= 2;
					hand_copy[k] -= 2;
					Search_Best_Combos(hand_copy, *pn, node_vec, L_min); find = 1;
				}
			}
		}
	}
	// 四带二张
	for (int i = 0; i < 13; ++i)
	{
		if (hand_deck[i] != 4)
			continue;
		for (int j = 0; j < 13; ++j)
		{
			if (j == i)
				continue;
			if (hand_deck[j] < 1)
				continue;
			for (int k = j + 1; k < 13; ++k)
			{
				if (k == i)
					continue;
				if (hand_deck[k] < 1)
					continue;
				// QUADRUPLE2
				Level* pl = new Level[3];
				pl[1] = j;
				pl[2] = k;
				CardCombo* pc = new CardCombo(CardComboType::QUADRUPLE2, i, 1, pl);
				my_node* pn = new my_node(pc, node.L + 1);
				pn->parent = &node;
				int hand_copy[15];
				memcpy(hand_copy, hand_deck, 60);
				hand_copy[i] = 0;
				hand_copy[j] -= 1;
				hand_copy[k] -= 1;
				Search_Best_Combos(hand_copy, *pn, node_vec, L_min); find = 1;
			}
		}
	}
	// 2连小翼飞机
	for (int i = 0; i < 11; ++i)
	{
		if (hand_deck[i] < 3 || hand_deck[i + 1] < 3)
			continue;
		// PLANE1
		for (int j = 0; j < 13; ++j)
		{
			if (j == i || j == i + 1)
				continue;
			if (hand_deck[j] < 1)
				continue;
			for (int k = j + 1; k < 13; ++k)
			{
				if (k == i || k == i + 1)
					continue;
				if (hand_deck[k] < 1)
					continue;
				Level* pl = new Level[3];
				pl[1] = j;
				pl[2] = k;
				CardCombo* pc = new CardCombo(CardComboType::PLANE1, i + 1, 2, pl);
				my_node* pn = new my_node(pc, node.L + 1);
				pn->parent = &node;
				memcpy(hand_copy, hand_deck, 60);
				Update_Hand_Deck(*pc, hand_copy);
				Search_Best_Combos(hand_copy, *pn, node_vec, L_min); find = 1;
			}
		}
	}
	// 2连大翼飞机
	for (int i = 0; i < 11; ++i)
	{
		if (hand_deck[i] < 3 || hand_deck[i + 1] < 3)
			continue;
		// PLANE2
		for (int j = 0; j < 13; ++j)
		{
			if (j == i || j == i + 1)
				continue;
			if (hand_deck[j] < 2)
				continue;
			for (int k = j + 1; k < 13; ++k)
			{
				if (k == i || k == i + 1)
					continue;
				if (hand_deck[k] < 2)
					continue;
				Level* pl = new Level[3];
				pl[1] = j;
				pl[2] = k;
				CardCombo* pc = new CardCombo(CardComboType::PLANE2, i + 1, 2, pl);
				my_node* pn = new my_node(pc, node.L + 1);
				pn->parent = &node;
				memcpy(hand_copy, hand_deck, 60);
				Update_Hand_Deck(*pc, hand_copy);
				Search_Best_Combos(hand_copy, *pn, node_vec, L_min); find = 1;
			}
		}
	}
	// 3连小翼飞机
	for (int i = 0; i < 10; ++i)
	{
		if (hand_deck[i] < 3 || hand_deck[i + 1] < 3 || hand_deck[i + 2] < 3)
			continue;
		// PLANE1
		for (int j = 0; j < 13; ++j)
		{
			if (j == i || j == i + 1 || j == i + 2)
				continue;
			if (hand_deck[j] < 1)
				continue;
			for (int k = j + 1; k < 13; ++k)
			{
				if (k == i || k == i + 1 || k == i + 2)
					continue;
				if (hand_deck[k] < 1)
					continue;
				for (int q = k + 1; q < 13; ++q)
				{
					if (q == i || q == i + 1 || q == i + 2)
						continue;
					if (hand_deck[q] < 1)
						continue;
					Level* pl = new Level[4];
					pl[1] = j;
					pl[2] = k;
					pl[3] = q;
					CardCombo* pc = new CardCombo(CardComboType::PLANE1, i + 2, 3, pl);
					my_node* pn = new my_node(pc, node.L + 1);
					pn->parent = &node;
					memcpy(hand_copy, hand_deck, 60);
					Update_Hand_Deck(*pc, hand_copy); find = 1;
					Search_Best_Combos(hand_copy, *pn, node_vec, L_min);
				}
			}
		}
	}
	// 3连大翼飞机
	for (int i = 0; i < 10; ++i)
	{
		if (hand_deck[i] < 3 || hand_deck[i + 1] < 3 || hand_deck[i + 2] < 3)
			continue;
		// PLANE2
		for (int j = 0; j < 13; ++j)
		{
			if (j == i || j == i + 1 || j == i + 2)
				continue;
			if (hand_deck[j] < 2)
				continue;
			for (int k = j + 1; k < 13; ++k)
			{
				if (k == i || k == i + 1 || k == i + 2)
					continue;
				if (hand_deck[k] < 2)
					continue;
				for (int q = k + 1; q < 13; ++q)
				{
					if (q == i || q == i + 1 || q == i + 2)
						continue;
					if (hand_deck[q] < 2)
						continue;
					Level* pl = new Level[4];
					pl[1] = j;
					pl[2] = k;
					pl[3] = q;
					CardCombo* pc = new CardCombo(CardComboType::PLANE2, i + 2, 3, pl);
					my_node* pn = new my_node(pc, node.L + 1);
					pn->parent = &node;
					memcpy(hand_copy, hand_deck, 60);
					Update_Hand_Deck(*pc, hand_copy); find = 1;
					Search_Best_Combos(hand_copy, *pn, node_vec, L_min);
				}
			}
		}
	}

	if (find == 0)
	{
		//std::cout << "find==0\n";
		int count = 0;
		for (int i = 0; i < 13; ++i)
			if (hand_deck[i])
				++count;
		if (node.L + count > L_min + features)
			return;
		my_node* prev = &node;
		for (int i = 0; i < 13; ++i)
		{
			CardCombo* pc = 0;
			if (hand_deck[i] == 1)
				pc = new CardCombo(CardComboType::SINGLE, i);
			else  if (hand_deck[i] == 2)
				pc = new CardCombo(CardComboType::PAIR, i);
			else
				continue;
			my_node* pn = new my_node(pc, prev->L + 1);
			pn->parent = prev;
			prev = pn;
		}
		L_min = std::min(L_min, prev->L);
		node_vec.push_back(prev);
	}
	//else
	//{
	//	// 对子
	//	for (int i = 0; i < 13; ++i)
	//	{
	//		if (hand_deck[i] < 2)
	//			continue;
	//		CardCombo* pc = new CardCombo(CardComboType::PAIR, i);
	//		my_node* pn = new my_node(pc, node.L + 1);
	//		pn->parent = &node;
	//		int hand_copy[15];
	//		memcpy(hand_copy, hand_deck, 60);
	//		hand_copy[i] -= 2;
	//		Search_Best_Combos(hand_copy, *pn, node_vec, L_min); find = 1;
	//	}
	//	 //单牌
	//	for (int i = 0; i < 13; ++i)
	//	{
	//		if (hand_deck[i] == 0)
	//			continue;
	//		CardCombo* pc = new CardCombo(CardComboType::SINGLE, i);
	//		my_node* pn = new my_node(pc, node.L + 1);
	//		pn->parent = &node;
	//		int hand_copy[15];
	//		memcpy(hand_copy, hand_deck, 60);
	//		hand_copy[i] -= 1;
	//		Search_Best_Combos(hand_copy, *pn, node_vec, L_min); find = 1;
	//	}
	//}
	return;
}
int Set_Best_Combos(int hand_deck[], set<CardCombo>& best_Combos) 
// 根据特征给出较好的出牌组合
{
	/*cout << "my hand_deck : ";
	for (int gsy = 0; gsy < 15; ++gsy)
		cout << hand_deck[gsy] << " ";
	cout << endl;*/
	int hand_copy[15];
	memcpy(hand_copy, hand_deck, 60);
	if (hand_copy[13] && hand_copy[14])
		best_Combos.insert(CardCombo(CardComboType::ROCKET));
	if (hand_copy[14])
		best_Combos.insert(CardCombo(CardComboType::SINGLE, 14));
	if (hand_copy[13])
		best_Combos.insert(CardCombo(CardComboType::SINGLE, 13));
	hand_copy[14] = 0;
	hand_copy[13] = 0;
	my_node init;
	vector<my_node*> node_vec;
	int L_min = 10000;
	//my_print(hand_deck, 15);
	//system("pause");
	Search_Best_Combos(hand_copy, init, node_vec, L_min);
	//std::cout << init.L << "\n";
	my_node* tmp;
	for (auto i : node_vec)
	{
		if (i->L > L_min + features)
			continue;
		tmp = i;
		while (tmp->L != 0)
		{
			if (tmp->use == 0)
			{
				//std::cout << tmp->c <<std::endl;
				best_Combos.insert(*(tmp->c));
				//std::cout << cardComboStrings[(int)tmp->c->comboType] << " " << tmp->c->comboLevel << '\n';
				tmp->use = 1;
			}
			tmp = tmp->parent;
		}
	}
	for (int i = 0; i < 13; ++i)
	{
		if (hand_deck[i]>0)
		{
			CardCombo* pc = new CardCombo(CardComboType::SINGLE, i);
			best_Combos.insert(*pc);
		}
		if (hand_deck[i] == 3 )
		{
			CardCombo* pc = new CardCombo(CardComboType::PAIR, i);
			best_Combos.insert(*pc);
		}
	}
	return L_min;
}
int Set_Best_Combos_SP(int hand_deck[], set<CardCombo>& best_Combos)
// 先手，根据特征给出较好的出牌组合
{
	int hand_copy[15];
	memcpy(hand_copy, hand_deck, 60);
	if (hand_copy[13] && hand_copy[14])
		best_Combos.insert(CardCombo(CardComboType::ROCKET));
	else if (hand_copy[14])
		best_Combos.insert(CardCombo(CardComboType::SINGLE, 14));
	else if (hand_copy[13])
		best_Combos.insert(CardCombo(CardComboType::SINGLE, 13));
	hand_copy[14] = 0;
	hand_copy[13] = 0;
	my_node init;
	vector<my_node*> node_vec;
	int L_min = 10000;
	Search_Best_Combos(hand_copy, init, node_vec, L_min);
	my_node* tmp;
	for (auto i : node_vec)
	{
		if (i->L > L_min)
			continue;
		tmp = i;
		while (tmp->L != 0)
		{
			if (tmp->use == 0)
			{
				best_Combos.insert(*(tmp->c));
				tmp->use = 1;
			}
			tmp = tmp->parent;
		}
	}
	return L_min;
}
void Update_Best_Combos(set<CardCombo>::iterator MyAction, int hand_deck[], set<CardCombo>& best_Combos)
{
	int s = MyAction->packs.size();
	for (int i = 0; i < s; ++i)
		hand_deck[MyAction->packs[i].level] -= MyAction->packs[i].count;
	best_Combos.erase(MyAction);
	vector<set<CardCombo>::iterator> v;
	for (set<CardCombo>::iterator it = best_Combos.begin(); it != best_Combos.end(); ++it)
	{
		s = it->packs.size();
		for (int i = 0; i < s; ++i)
		{
			if (hand_deck[it->packs[i].level] < it->packs[i].count)
			{
				// conflict
				v.push_back(it);
				break;
			}
		}
	}
	for (auto i : v)
		best_Combos.erase(i);
	return;
}
void Set_Card(int Card_to_Allocate[], int hand_deck[][15], int num_of_card[]) {
	for (int i = 0; i < 3; i++)
		num_of_card[i] = cardRemaining[i];
	//用于记录地主牌已经被打出的地主牌
	set<Card> used_landloadpubliccard;
	//将待分配的牌设置为整副牌
	//设置Level为0到12的牌（即3到2的牌）
	for (int i = 0; i < 13; i++) {
		Card_to_Allocate[i] = 4;
	}
	//设置大小王
	Card_to_Allocate[13] = 1;
	Card_to_Allocate[14] = 1;
	//在待分配的牌中减去地主牌
	for (auto i = landlordPublicCards.begin(); i != landlordPublicCards.end(); i++) {
		Card_to_Allocate[card2level(*i)]--;
	}
	//在待分配的牌中减去已经出了的牌（注意这时需要跳过地主牌）
	for (int i = 0; i < 3; i++) {
		for (auto j = whatTheyPlayed[i].begin(); j != whatTheyPlayed[i].end(); j++) {
			for (auto k = j->begin(); k != j->end(); k++) {
				if (landlordPublicCards.count(*k)) {
					used_landloadpubliccard.insert(*k);
					continue;
				}
				Card_to_Allocate[card2level(*k)]--;
			}
		}
	}
	//将我的手牌换为标准格式并从待分配手牌中除去我的手牌(如果我是地主要注意跳过地主牌)
	for (auto i = myCards.begin(); i != myCards.end(); i++) {
		hand_deck[myPosition][card2level(*i)]++;
		if (myPosition != 0 || (!landlordPublicCards.count(*i)))
			Card_to_Allocate[card2level(*i)]--;
	}
	//将地主牌分发给地主
	if (myPosition != 0) {
		for (auto i = landlordPublicCards.begin(); i != landlordPublicCards.end(); i++) {
			if (!used_landloadpubliccard.count(*i)) {
				hand_deck[0][card2level(*i)]++;
				num_of_card[0]--;
			}
		}
	}

}
int last_valid_from() {
	if (whatTheyPlayed[(myPosition - 1 + 3) % 3].back().empty())
		return (myPosition - 2 + 3) % 3;
	return (myPosition - 1 + 3) % 3;
}
int caul_card_num(int imagined_hand_deck[15]) {
	int sum = 0;
	for (int i = 0; i < 15; i++) {
		sum += imagined_hand_deck[i];
	}
	return sum;
}
void Update_Hand_deck(const CardCombo& action, int* hand_deck) {
	if (action.comboType == CardComboType::PASS || action.comboType == CardComboType::INVALID)
		return;
	for (auto i = action.packs.begin(); i != action.packs.end(); i++) {
		hand_deck[(*i).level] -= (*i).count;
	}
	return;
}
//返回完成发牌与第一次的决策后的输赢
int result_of_one_imagined_game(int imagined_hand_deck[][15], CardCombo last_valid, int turn_of_who, int this_valid_combo_from) {
	int last_one = (turn_of_who - 1 + 3) % 3;
	CardCombo empty;
	if (this_valid_combo_from == turn_of_who) {
		last_valid = empty;
		this_valid_combo_from = (this_valid_combo_from - 2 + 3) % 3;
	}
	/*cout << "**************************************" << endl;
	cout << "this combo from " << this_valid_combo_from << endl << endl;
	cout << "last_one is " << last_one << endl << endl;
	cout << "It turns to " << turn_of_who << endl << endl;
	cout << "last valid is " << endl;
	if (last_valid.comboType == CardComboType::PASS)
		cout << "PASS" << endl;
	else {
		for (int i = 0; i < last_valid.packs.size(); i++)
			for (int j = 1; j <= last_valid.packs[i].count; j++)
				cout << last_valid.packs[i].level << " ";
		cout << endl;
	}
	cout << endl;
	cout << "right now the deck of all is " << endl;
	for (int i = 0; i < 3; i++) {
		cout << i << " : ";
		for (int j = 0; j < 15; j++) {
			cout << imagined_hand_deck[i][j] << " ";
		}
		cout << endl;
	}*/
	//判断游戏是否已经结束
	if (caul_card_num(imagined_hand_deck[last_one]) == 0) {
		if (last_one == myPosition) {
			if (myPosition == 0)
				return 2;
			else
				return 1;
		}
		else if (last_one == 0)
			return -1;
		else {
			if (myPosition + last_one == 3)
				return 1;
			else
				return -2;
		}
	}
	CardCombo action;
	action = last_valid.findValid(imagined_hand_deck, turn_of_who, this_valid_combo_from);
	Update_Hand_deck(action, imagined_hand_deck[turn_of_who]);
	/*cout << "It turns to " << turn_of_who << " ,and this game isn't ended" << endl;
	cout << "He decides to ";
	if (action.comboType == CardComboType::PASS) {
		cout << "PASS" << endl;
	}
	else {
		for (int i = 0; i < action.packs.size(); i++)
			for (int j = 1; j <= action.packs[i].count; j++)
				cout << action.packs[i].level << " ";
		cout << endl;
	}*/
	if (action.comboType == CardComboType::PASS)
		return result_of_one_imagined_game(imagined_hand_deck, last_valid, (turn_of_who + 1) % 3, this_valid_combo_from);
	return result_of_one_imagined_game(imagined_hand_deck, action, (turn_of_who + 1) % 3, turn_of_who);
}
//返回在一次随机发牌之后的表现最优的action的编号
vector<Card> Give_Best_Combo() {
	int Card_to_Allocate[15];
	int hand_deck[3][15];
	int num_of_card[3] = { 0, 0, 0 };
	memset(Card_to_Allocate, 0, 15 * sizeof(int));
	memset(hand_deck, 0, 3 * 15 * sizeof(int));
	Set_Card(Card_to_Allocate, hand_deck, num_of_card);
	/*cout << "My position is " << myPosition << endl << endl;
	cout << "Card to Allocate:" << endl;
	for (int i = 0; i < 15; i++) {
		cout << Card_to_Allocate[i] << " ";
	}
	cout << endl << endl;
	cout << "num_of_card:" << endl;
	for (int i = 0; i < 3; i++) {
		cout << num_of_card[i] << " ";
	}
	cout << endl << endl;
	cout << "hand_deck:" << endl;
	for (int i = 0; i < 3; i++) {
		cout << i << " : ";
		for (int j = 0; j < 15; j++) {
			cout << hand_deck[i][j] << " ";
		}
		cout << endl;
	}
	cout << endl;
	cout << "lastValidCombo is" << endl;
	if (lastValidCombo.comboType == CardComboType::PASS)
		cout << "PASS" << endl;
	else {
		for (int i = 0; i < lastValidCombo.packs.size(); i++)
			for (int j = 1; j <= lastValidCombo.packs[i].count; j++)
				cout << lastValidCombo.packs[i].level << " ";
		cout << endl;
	}
	cout << endl;*/
	set<CardCombo> best_Combos;
	Set_Best_Combos_SP(hand_deck[myPosition], best_Combos);
	/*cout << best_Combos.size() << endl;*/
	vector<CardCombo> alternative_action;
	vector<int*> alternative_hand_deck;
	CardCombo empty;
	alternative_action.push_back(empty);
	int toolman[15];
	memcpy(toolman, hand_deck[myPosition], 15 * sizeof(int));
	alternative_hand_deck.push_back(toolman);
	for (auto i = best_Combos.begin(); i != best_Combos.end(); i++) {
		if (lastValidCombo.canBeBeatenBy(*i)) {
			int* imagined_my_hand_deck = new int[15];
			memcpy(imagined_my_hand_deck, hand_deck[myPosition], 15 * sizeof(int));
			Update_Hand_deck(*i, imagined_my_hand_deck);
			alternative_hand_deck.push_back(imagined_my_hand_deck);
			alternative_action.push_back(*i);
		}
	}
	//cout <<"There are "<< alternative_action.size() <<" valid actions. They are "<< endl;
	//int count = 0;
	//for (auto i = alternative_action.begin(); i != alternative_action.end(); i++) {
	//	cout << count++ << " : ";
	//	if ((*i).comboType == CardComboType::PASS) {
	//		//cout << "PASS" << endl;
	//		continue;
	//	}
	//	for (int j = 0; j < (*i).packs.size(); j++) {
	//		for (int k = 1; k <= (*i).packs[j].count; k++)
	//			cout << (*i).packs[j].level << " ";
	//	}
	//	cout << endl;
	//}
	//cout << endl;
	/*cout << "After play these combos, my hand deck is " << endl;
	for (auto i = alternative_hand_deck.begin(); i != alternative_hand_deck.end(); i++) {
		for (int j = 1; j < 15; j++)
			cout << (*i)[j] << " ";
		cout << endl;
	}
	cout << endl;*/
	int this_combo_from =0;
	CardCombo last_valid;

	int times = 1;
	for (times=1; (end_of_time - start)*1.0 / CLOCKS_PER_SEC <= 0.9;times++) {	
		/*cout << "This is the " << times << "th to randomly allocate card" << endl;*/
		memcpy(temp_hand_deck, hand_deck, 3 * 15 * sizeof(int));
		Guess_Others_Hand_Deck(temp_hand_deck, num_of_card, myPosition, Card_to_Allocate);
		for (int j = 0; j < alternative_action.size(); j++) {
			if (lastValidCombo.comboType == CardComboType::PASS && j == 0)
				continue;
			if (alternative_action[j].comboType != CardComboType::PASS) {
				last_valid = alternative_action[j];
				this_combo_from = myPosition;
			}
			else {
				last_valid = lastValidCombo;
				this_combo_from = last_valid_from();
			}
			memcpy(imagined_hand_deck[(myPosition + 2) % 3], temp_hand_deck[(myPosition + 2) % 3], 15 * sizeof(int));
			memcpy(imagined_hand_deck[(myPosition + 1) % 3], temp_hand_deck[(myPosition + 1) % 3], 15 * sizeof(int));
			memcpy(imagined_hand_deck[myPosition], alternative_hand_deck[j],15 * sizeof(int));
			/*cout << "action " << j << endl;
			cout << "imagined hand deck:" << endl;
			for (int i = 0; i < 3; i++) {
				cout << i << " : ";
				for (int j = 0; j < 15; j++) {
					cout << imagined_hand_deck[i][j] << " ";
				}
				cout << endl;
			}*/
			grade[j] += result_of_one_imagined_game(imagined_hand_deck, last_valid, (myPosition + 1) % 3, this_combo_from);
		}
		end_of_time = clock();
	}
	int max_grade = -1008600;
	int max_num = 0;
	for (int i = 0; i < alternative_action.size(); i++) {
		if (lastValidCombo.comboType == CardComboType::PASS && i == 0)
			continue;

	//	cout << "the score of action " << i << " is " << grade[i] << endl;

		if (grade[i] >= max_grade) {
			max_num = i;
			max_grade = grade[i];
		}
	}
	vector<Card> My_action;
	if (max_num == 0)
		return My_action;
	int num_of_pack = alternative_action[max_num].packs.size();
	for (int i = 0; i < num_of_pack; i++) {
		int k = 0;
		for (auto j = myCards.begin(); k < alternative_action[max_num].packs[i].count && j != myCards.end(); j++) {
			if (card2level(*j) == alternative_action[max_num].packs[i].level) {
				k++;
				My_action.push_back(*j);
			}
		}
	}
	return My_action;
}

int main()
{
	int d[15] = {};
	//Set_Best_Combos_SP(int hand_deck[], set<CardCombo>&best_Combos)
	srand(time(nullptr));
	BotzoneIO::read();
	start = clock();
	/*for (auto i = myCards.begin(); i != myCards.end(); i++) {
		cout << card2level(*i) << " ";
	}
	cout << endl;
	cout << myPosition << endl;
	cout << landlordPosition << endl;*/
	if (stage == Stage::BIDDING)
	{
		// 做出决策（你只需修改以下部分）
		auto maxBidIt = std::max_element(bidInput.begin(), bidInput.end());
		int maxBid = maxBidIt == bidInput.end() ? -1 : *maxBidIt;
		int my_hand_deck[15];
		memset(my_hand_deck, 0, 15 * sizeof(int));
		for (auto i = myCards.begin(); i != myCards.end(); i++)
			my_hand_deck[card2level(*i)]++;
		set<CardCombo> best_combos;
		int L_min = Set_Best_Combos_SP(my_hand_deck, best_combos);
		/*for (auto s : best_combos)
			cout << cardComboStrings[(int)s.comboType]<<"  "<<s.comboLevel<< endl;
		cout << L_min << endl;*/
		int bidValue = My_Bid_Brave(best_combos, maxBid, L_min,my_hand_deck);
		BotzoneIO::bid(bidValue);
	}
	else if (stage == Stage::PLAYING)
	{
		// 做出决策（你只需修改以下部分）
		// findFirstValid 函数可以用作修改的起点
		vector<Card> action = Give_Best_Combo();
		//CardCombo myAction(action.begin(), action.end());
		//// 是合法牌
		//assert(myAction.comboType != CardComboType::INVALID);
		//assert(
		//	// 在上家没过牌的时候过牌
		//	(lastValidCombo.comboType != CardComboType::PASS && myAction.comboType == CardComboType::PASS) ||
		//	// 在上家没过牌的时候出打得过的牌
		//	(lastValidCombo.comboType != CardComboType::PASS && lastValidCombo.canBeBeatenBy(myAction)) ||
		//	// 在上家过牌的时候出合法牌
		//	(lastValidCombo.comboType == CardComboType::PASS && myAction.comboType != CardComboType::INVALID));
		//// 决策结束，输出结果（你只需修改以上部分）
		BotzoneIO::play(action.begin(), action.end());
	}
	return 0;
}

CardCombo CardCombo::findValid(int hand_deck[][15], int My_position, int this_combo_from)
{
	Level csingle[3][15] = { {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1} };
	Level cpair[3][15] = { {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1} };
	Level ctriplet[3][15] = { {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1} };
	Level cstraight_5[3][15] = { {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1} };
	Level cstraight_6[3][15] = { {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1} };
	Level cstraight_7[3][15] = { {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1} };
	Level cstraight_8[3][15] = { {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1} };
	Level cstraight_9[3][15] = { {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1} };
	Level cstraight2_3[3][15] = { {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1} };
	Level cstraight2_4[3][15] = { {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1} };
	Level cstraight2_5[3][15] = { {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1} };
	Level cstraight2_6[3][15] = { {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1} };
	Level cbomb[3][15] = { {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1} };
	Level cstraight3_2[3][15] = { {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1} };
	Level cstraight3_3[3][15] = { {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1} };
	Level cstraight3_4[3][15] = { {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1} };
	bool rocket[3] = { 0, 0, 0 };

	int csingle_num[3] = { 0,0,0 };
	int cpair_num[3] = { 0,0,0 };
	int ctriplet_num[3] = { 0,0,0 };
	int cstraight_5_num[3] = { 0,0,0 };
	int cstraight_6_num[3] = { 0,0,0 };
	int cstraight_7_num[3] = { 0,0,0 };
	int cstraight_8_num[3] = { 0,0,0 };
	int cstraight_9_num[3] = { 0,0,0 };
	int cstraight2_3_num[3] = { 0,0,0 };
	int cstraight2_4_num[3] = { 0,0,0 };
	int cstraight2_5_num[3] = { 0,0,0 };
	int cstraight2_6_num[3] = { 0,0,0 };
	int cbomb_num[3] = { 0,0,0 };
	int cstraight3_2_num[3] = { 0,0,0 };
	int cstraight3_3_num[3] = { 0,0,0 };
	int cstraight3_4_num[3] = { 0,0,0 };


	for (int person = 0; person <= 2; person++)
	{
		for (int i = 0; i < 15; i++)
		{
			if (hand_deck[person][i] == 1)
			{
				csingle_num[person]++;
				csingle[person][csingle_num[person]] = i;
			}
			else if (hand_deck[person][i] == 2)
			{
				cpair_num[person]++;
				cpair[person][cpair_num[person]] = i;
			}
			else if (hand_deck[person][i] == 3)
			{
				ctriplet_num[person]++;
				ctriplet[person][ctriplet_num[person]] = i;
			}
			else if (hand_deck[person][i] == 4)
			{
				cbomb_num[person]++;
				cbomb[person][cbomb_num[person]] = i;
			}
		}
		for (int i = 0; i < 12; i++)
		{
			for (int j = 0; j < 9; j++)
			{
				if (i + j >= 12)break;
				if (hand_deck[person][i + j] == 0)
				{
					break;
				}
				if (j == 4)
				{
					cstraight_5_num[person]++;
					cstraight_5[person][cstraight_5_num[person]] = i + j;
				}
				if (j == 5)
				{
					cstraight_6_num[person]++;
					cstraight_6[person][cstraight_6_num[person]] = i + j;
				}
				if (j == 6)
				{
					cstraight_7_num[person]++;
					cstraight_7[person][cstraight_7_num[person]] = i + j;
				}
				if (j == 7)
				{
					cstraight_8_num[person]++;
					cstraight_8[person][cstraight_8_num[person]] = i + j;
				}
				if (j == 8)
				{
					cstraight_9_num[person]++;
					cstraight_9[person][cstraight_9_num[person]] = i + j;
				}
			}
			for (int j = 0; j < 9; j++)
			{
				if (i + j >= 12)break;
				if (hand_deck[person][i + j] < 2)
				{
					break;
				}
				if (j == 2)
				{
					cstraight2_3_num[person]++;
					cstraight2_3[person][cstraight2_3_num[person]] = i + j;
				}
				if (j == 3)
				{
					cstraight2_4_num[person]++;
					cstraight2_4[person][cstraight2_4_num[person]] = i + j;
				}
				if (j == 4)
				{
					cstraight2_5_num[person]++;
					cstraight2_5[person][cstraight2_5_num[person]] = i + j;
				}
				if (j == 5)
				{
					cstraight2_6_num[person]++;
					cstraight2_6[person][cstraight2_6_num[person]] = i + j;
				}
			}
			for (int j = 0; j < 9; j++)
			{
				if (i + j >= 12)break;
				if (hand_deck[person][i + j] < 3)
				{
					break;
				}
				if (j == 1)
				{
					cstraight3_2_num[person]++;
					cstraight3_2[person][cstraight3_2_num[person]] = i + j;
				}
				if (j == 2)
				{
					cstraight3_3_num[person]++;
					cstraight3_3[person][cstraight3_3_num[person]] = i + j;
				}
				if (j == 3)
				{
					cstraight3_4_num[person]++;
					cstraight3_4[person][cstraight3_4_num[person]] = i + j;
				}
			}
		}
		if (csingle[person][13] == 1 && csingle[person][14] == 1)
		{
			rocket[person] = 1;
		}
	}
	//debug
	/*for (int j = 0; j <= 2; j++)
	{
		cout << "-----------------triplet------------------------------" << endl;
		cout << ctriplet_num[j] << endl;
		for (int i = 0; i <= 14; i++)
		{
			cout << ctriplet[j][i] << " ";

		}
		cout << endl;
		cout << "-----------------single------------------------------" << endl;
		cout << csingle_num[j] << endl;
		for (int i = 0; i <= 14; i++)
		{
			cout << csingle[j][i] << " ";

		}
		cout << endl;
		cout << "-----------------cstraight_5------------------------------" << endl;
		cout << cstraight_5_num[j] << endl;
		for (int i = 0; i <= 14; i++)
		{
			cout << cstraight_5[j][i] << " ";

		}
		cout << endl;
		cout << "-----------------cstraight2_3------------------------------" << endl;
		cout << cstraight2_3_num[j] << endl;
		for (int i = 0; i <= 14; i++)
		{
			cout << cstraight2_3[j][i] << " ";

		}
		cout << endl;
	}*/
	//身份判别
	if (My_position == 0)//地主
	{
		if (comboType == CardComboType::PASS)
		{
			if (cstraight2_6_num[My_position] > 0)
			{
				return CardCombo(CardComboType::STRAIGHT2,
					cstraight2_6[My_position][1], 6);
			}
			if (cstraight2_5_num[My_position] > 0)
			{
				return CardCombo(CardComboType::STRAIGHT2,
					cstraight2_5[My_position][1], 5);
			}
			if (cstraight2_4_num[My_position] > 0)
			{
				return CardCombo(CardComboType::STRAIGHT2,
					cstraight2_4[My_position][1], 4);
			}
			if (cstraight2_3_num[My_position] > 0)
			{
				return CardCombo(CardComboType::STRAIGHT2,
					cstraight2_3[My_position][1], 3);
			}
			if (cstraight_9_num[My_position] > 0)
			{
				return CardCombo(CardComboType::STRAIGHT,
					cstraight_9[My_position][1], 9);
			}
			if (cstraight_8_num[My_position] > 0)
			{
				return CardCombo(CardComboType::STRAIGHT,
					cstraight_8[My_position][1], 8);
			}
			if (cstraight_7_num[My_position] > 0)
			{
				return CardCombo(CardComboType::STRAIGHT,
					cstraight_7[My_position][1], 7);
			}
			if (cstraight_6_num[My_position] > 0)
			{
				return CardCombo(CardComboType::STRAIGHT,
					cstraight_6[My_position][1], 6);
			}
			if (cstraight_5_num[My_position] > 0)
			{
				return CardCombo(CardComboType::STRAIGHT,
					cstraight_5[My_position][1], 5);
			}
			if (cstraight3_4_num[My_position] > 0)
			{
				if (csingle_num[My_position] >= 4)
				{
					return CardCombo(CardComboType::PLANE1,
						cstraight3_4[My_position][1], 4, csingle[My_position]);
				}
				if (cpair_num[My_position] >= 4)
				{
					return CardCombo(CardComboType::PLANE2,
						cstraight3_4[My_position][1], 4, cpair[My_position]);
				}
			}
			if (cstraight3_3_num[My_position] > 0)
			{
				if (csingle_num[My_position] >= 3)
				{
					return CardCombo(CardComboType::PLANE1,
						cstraight3_3[My_position][1], 3, csingle[My_position]);
				}
				else if (cpair_num[My_position] >= 3)
				{
					return CardCombo(CardComboType::PLANE2,
						cstraight3_3[My_position][1], 3, cpair[My_position]);
				}
			}
			if (cstraight3_2_num[My_position] > 0)
			{
				if (csingle_num[My_position] >= 2)
				{
					return CardCombo(CardComboType::PLANE1,
						cstraight3_2[My_position][cstraight3_2_num[My_position]], 2, csingle[My_position]);
				}
				if (cpair_num[My_position] >= 2)
				{
					return CardCombo(CardComboType::PLANE2,
						cstraight3_2[My_position][cstraight3_2_num[My_position]], 2, cpair[My_position]);
				}
			}
			if (ctriplet_num[My_position] > 0)
			{
				if (csingle_num[My_position] > 0)
				{
					return CardCombo(CardComboType::TRIPLET1,
						ctriplet[My_position][1], 1, csingle[My_position]);
				}
				if (cpair_num[My_position] > 0)
				{
					return CardCombo(CardComboType::TRIPLET2,
						ctriplet[My_position][1], 1, cpair[My_position]);
				}
				return CardCombo(CardComboType::TRIPLET,
					ctriplet[My_position][1], 1);
			}
			if (cstraight3_4_num[My_position] > 0)
			{
				return CardCombo(CardComboType::PLANE,
					cstraight3_4[My_position][cstraight3_4_num[My_position]], 4);
			}
			if (cstraight3_3_num[My_position] > 0)
			{
				return CardCombo(CardComboType::PLANE,
					cstraight3_3[My_position][cstraight3_3_num[My_position]], 3);
			}
			if (cstraight3_2_num[My_position] > 0)
			{
				return CardCombo(CardComboType::PLANE,
					cstraight3_2[My_position][cstraight3_2_num[My_position]], 2);
			}
			if (cpair_num[My_position] != 0)
			{
				return CardCombo(CardComboType::PAIR,
					cpair[My_position][1], 1);
			}
			if (csingle_num[My_position] != 0)
			{
				return CardCombo(CardComboType::SINGLE,
					csingle[My_position][1], 1);
			}
			if (cbomb_num[My_position] != 0)
			{
				return CardCombo(CardComboType::BOMB,
					cbomb[My_position][1], 1);
			}
			if (rocket[My_position] != 0)
			{
				return CardCombo(CardComboType::ROCKET,
					15, 1);
			}
		}
		else if (comboType == CardComboType::SINGLE)
		{
			if (csingle[My_position][csingle_num[My_position]] > comboLevel)
			{
				if (csingle[My_position][csingle_num[My_position]] >= csingle[1][csingle_num[1]] &&
					csingle[My_position][csingle_num[My_position]] >= csingle[2][csingle_num[2]])
				{
					for (int j = 1; j <= csingle_num[My_position]; j++)
					{
						if (csingle[My_position][j] >= csingle[1][csingle_num[1]] && csingle[My_position][j] >= csingle[2][csingle_num[2]])
						{
							return CardCombo(CardComboType::SINGLE,
								csingle[My_position][j], 1);
						}
					}
				}
				else
				{
					for (int j = 1; j <= csingle_num[My_position]; j++)
					{
						if (csingle[My_position][j] > comboLevel)
						{
							return CardCombo(CardComboType::SINGLE,
								csingle[My_position][j], 1);
						}
					}
				}
			}
			return CardCombo(CardComboType::PASS);
		}
		else if (comboType == CardComboType::PAIR)
		{
			if (cpair[My_position][cpair_num[My_position]] > comboLevel)
			{

				if (cpair[My_position][cpair_num[My_position]] >= cpair[1][cpair_num[1]] &&
					cpair[My_position][cpair_num[My_position]] >= cpair[2][cpair_num[2]])
				{
					for (int j = 1; j <= cpair_num[My_position]; j++)
					{
						if (cpair[My_position][j] >= cpair[1][cpair_num[1]] && cpair[My_position][j] >= cpair[2][cpair_num[2]] &&
							cpair[My_position][j] > comboLevel)
						{
							return CardCombo(CardComboType::PAIR,
								cpair[My_position][j], 1);
						}
					}
				}

				else
				{
					for (int j = 1; j <= cpair_num[My_position]; j++)
					{
						if (cpair[My_position][j] > comboLevel)
						{
							return CardCombo(CardComboType::PAIR,
								cpair[My_position][j], 1);
						}
					}
				}
			}
			return CardCombo(CardComboType::PASS);
		}
		else if (comboType == CardComboType::TRIPLET)
		{
			if (ctriplet[My_position][ctriplet_num[My_position]] > comboLevel)
			{

				if (ctriplet[My_position][ctriplet_num[My_position]] >= ctriplet[1][ctriplet_num[1]] &&
					ctriplet[My_position][ctriplet_num[My_position]] >= ctriplet[2][ctriplet_num[2]])
				{
					for (int j = 1; j <= ctriplet_num[My_position]; j++)
					{
						if (ctriplet[My_position][j] >= ctriplet[1][ctriplet_num[1]] &&
							ctriplet[My_position][j] >= ctriplet[2][ctriplet_num[2]] &&
							ctriplet[My_position][j] > comboLevel)
						{
							return CardCombo(CardComboType::TRIPLET,
								ctriplet[My_position][j], 1);
						}
					}
				}

				else
				{
					for (int j = 1; j <= ctriplet_num[My_position]; j++)
					{
						if (ctriplet[My_position][j] > comboLevel)
						{
							return CardCombo(CardComboType::TRIPLET,
								ctriplet[My_position][j], 1);
						}
					}
				}
			}
			return CardCombo(CardComboType::PASS);
		}
		else if (comboType == CardComboType::BOMB)
		{
			if (cbomb[My_position][cbomb_num[My_position]] > comboLevel)
			{

				if (cbomb[My_position][cbomb_num[My_position]] >= cbomb[1][cbomb_num[1]] &&
					cbomb[My_position][cbomb_num[My_position]] >= cbomb[2][cbomb_num[2]])
				{
					for (int j = 1; j <= cbomb_num[My_position]; j++)
					{
						if (cbomb[My_position][j] >= cbomb[1][cbomb_num[1]] && cbomb[My_position][j] >= cbomb[2][cbomb_num[2]]
							&& cbomb[My_position][j] > comboLevel)
						{
							return CardCombo(CardComboType::BOMB,
								cbomb[My_position][j], 1);
						}
					}

				}
				else
				{
					for (int j = 1; j <= cbomb_num[My_position]; j++)
					{
						if (cbomb[My_position][j] > comboLevel)
						{
							return CardCombo(CardComboType::BOMB,
								cbomb[My_position][j], 1);
						}
					}
				}
			}
			if (rocket[My_position] == 1)
			{
				return CardCombo(CardComboType::ROCKET);
			}
			return CardCombo(CardComboType::PASS);
		}
		else if (comboType == CardComboType::STRAIGHT)
		{
			if (packs.size() == 5)
			{
				if (cstraight_5[My_position][cstraight_5_num[My_position]] > comboLevel)
				{

					if (cstraight_5[My_position][cstraight_5_num[My_position]] >= cstraight_5[1][cstraight_5_num[1]] &&
						cstraight_5[My_position][cstraight_5_num[My_position]] >= cstraight_5[2][cstraight_5_num[2]])
					{
						for (int j = 1; j <= cstraight_5_num[My_position]; j++)
						{
							if (cstraight_5[My_position][j] >= cstraight_5[1][cstraight_5_num[1]] && cstraight_5[My_position][j] >= cstraight_5[2][cstraight_5_num[2]]
								&& cstraight_5[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::STRAIGHT,
									cstraight_5[My_position][j], 5);
							}
						}
					}

					else
					{
						for (int j = 1; j <= cstraight_5_num[My_position]; j++)
						{
							if (cstraight_5[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::STRAIGHT,
									cstraight_5[My_position][j], 5);
							}
						}
					}
				}
			}
			else if (packs.size() == 6)
			{
				if (cstraight_6[My_position][cstraight_6_num[My_position]] > comboLevel)
				{

					if (cstraight_6[My_position][cstraight_6_num[My_position]] >= cstraight_6[1][cstraight_6_num[1]] &&
						cstraight_6[My_position][cstraight_6_num[My_position]] >= cstraight_6[2][cstraight_6_num[2]])
					{
						for (int j = 1; j <= cstraight_6_num[My_position]; j++)
						{
							if (cstraight_6[My_position][j] >= cstraight_6[1][cstraight_6_num[1]] && cstraight_6[My_position][j] >= cstraight_6[2][cstraight_6_num[2]]
								&& cstraight_6[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::STRAIGHT,
									cstraight_6[My_position][j], 6);
							}
						}
					}

					else
					{
						for (int j = 1; j <= cstraight_6_num[My_position]; j++)
						{
							if (cstraight_6[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::STRAIGHT,
									cstraight_6[My_position][j], 6);
							}
						}
					}
				}
			}
			else if (packs.size() == 7)
			{
				if (cstraight_7[My_position][cstraight_7_num[My_position]] > comboLevel)
				{

					if (cstraight_7[My_position][cstraight_7_num[My_position]] >= cstraight_7[1][cstraight_7_num[1]] &&
						cstraight_7[My_position][cstraight_7_num[My_position]] >= cstraight_7[2][cstraight_7_num[2]])
					{
						for (int j = 1; j <= cstraight_7_num[My_position]; j++)
						{
							if (cstraight_7[My_position][j] >= cstraight_7[1][cstraight_7_num[1]] && cstraight_7[My_position][j] >= cstraight_7[2][cstraight_7_num[2]]
								&& cstraight_7[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::STRAIGHT,
									cstraight_7[My_position][j], 7);
							}
						}
					}

					else
					{
						for (int j = 1; j <= cstraight_7_num[My_position]; j++)
						{
							if (cstraight_7[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::STRAIGHT,
									cstraight_7[My_position][j], 7);
							}
						}
					}
				}
			}
			else if (packs.size() == 8)
			{
				if (cstraight_8[My_position][cstraight_8_num[My_position]] > comboLevel)
				{

					if (cstraight_8[My_position][cstraight_8_num[My_position]] >= cstraight_8[1][cstraight_8_num[1]] &&
						cstraight_8[My_position][cstraight_8_num[My_position]] >= cstraight_8[2][cstraight_8_num[2]])
					{
						for (int j = 1; j <= cstraight_8_num[My_position]; j++)
						{
							if (cstraight_8[My_position][j] >= cstraight_8[1][cstraight_8_num[1]] && cstraight_8[My_position][j] >= cstraight_8[2][cstraight_8_num[2]]
								&& cstraight_8[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::STRAIGHT,
									cstraight_8[My_position][j], 8);
							}
						}
					}

					else
					{
						for (int j = 1; j <= cstraight_8_num[My_position]; j++)
						{
							if (cstraight_8[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::STRAIGHT,
									cstraight_8[My_position][j], 8);
							}
						}
					}
				}
			}
			else if (packs.size() == 9)
			{
				if (cstraight_9[My_position][cstraight_9_num[My_position]] > comboLevel)
				{

					if (cstraight_9[My_position][cstraight_9_num[My_position]] >= cstraight_9[1][cstraight_9_num[1]] &&
						cstraight_9[My_position][cstraight_9_num[My_position]] >= cstraight_9[2][cstraight_9_num[2]])
					{
						for (int j = 1; j <= cstraight_9_num[My_position]; j++)
						{
							if (cstraight_9[My_position][j] >= cstraight_9[1][cstraight_9_num[1]] && cstraight_9[My_position][j] >= cstraight_9[2][cstraight_9_num[2]]
								&& cstraight_9[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::STRAIGHT,
									cstraight_9[My_position][j], 9);
							}
						}
					}

					else
					{
						for (int j = 1; j <= cstraight_9_num[My_position]; j++)
						{
							if (cstraight_9[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::STRAIGHT,
									cstraight_9[My_position][j], 9);
							}
						}
					}
				}
			}
			if (cbomb_num[My_position] > 0)
			{
				return CardCombo(CardComboType::BOMB,
					cbomb[My_position][1], 1);
			}
			if (rocket[My_position] == 1)
			{
				return CardCombo(CardComboType::ROCKET);
			}
			return CardCombo(CardComboType::PASS);
		}
		else if (comboType == CardComboType::STRAIGHT2)
		{
			if (packs.size() == 3)
			{
				if (cstraight2_3[My_position][cstraight2_3_num[My_position]] > comboLevel)
				{

					if (cstraight2_3[My_position][cstraight2_3_num[My_position]] >= cstraight2_3[1][cstraight2_3_num[1]] &&
						cstraight2_3[My_position][cstraight2_3_num[My_position]] >= cstraight2_3[2][cstraight2_3_num[2]])
					{
						for (int j = 1; j <= cstraight2_3_num[My_position]; j++)
						{
							if (cstraight2_3[My_position][j] >= cstraight2_3[1][cstraight2_3_num[1]] && cstraight2_3[My_position][j] >= cstraight2_3[2][cstraight2_3_num[1]]
								&& cstraight2_3[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::STRAIGHT2,
									cstraight2_3[My_position][j], 3);
							}
						}
					}

					else
					{
						for (int j = 1; j <= cstraight2_3_num[My_position]; j++)
						{
							if (cstraight2_3[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::STRAIGHT2,
									cstraight2_3[My_position][j], 3);
							}
						}
					}
				}
			}
			else if (packs.size() == 4)
			{
				if (cstraight2_4[My_position][cstraight2_4_num[My_position]] > comboLevel)
				{

					if (cstraight2_4[My_position][cstraight2_4_num[My_position]] >= cstraight2_4[1][cstraight2_4_num[1]] &&
						cstraight2_4[My_position][cstraight2_4_num[My_position]] >= cstraight2_4[2][cstraight2_4_num[2]])
					{
						for (int j = 1; j <= cstraight2_4_num[My_position]; j++)
						{
							if (cstraight2_4[My_position][j] >= cstraight2_4[1][cstraight2_4_num[1]] && cstraight2_4[My_position][j] >= cstraight2_4[2][cstraight2_4_num[2]]
								&& cstraight2_4[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::STRAIGHT2,
									cstraight2_4[My_position][j], 4);
							}
						}
					}

					else
					{
						for (int j = 1; j <= cstraight2_4_num[My_position]; j++)
						{
							if (cstraight2_4[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::STRAIGHT2,
									cstraight2_4[My_position][j], 4);
							}
						}
					}
				}
			}
			else if (packs.size() == 5)
			{
				if (cstraight2_5[My_position][cstraight2_5_num[My_position]] > comboLevel)
				{

					if (cstraight2_5[My_position][cstraight2_5_num[My_position]] >= cstraight2_5[1][cstraight2_5_num[1]] &&
						cstraight2_5[My_position][cstraight2_5_num[My_position]] >= cstraight2_5[2][cstraight2_5_num[2]])
					{
						for (int j = 1; j <= cstraight2_5_num[My_position]; j++)
						{
							if (cstraight2_5[My_position][j] >= cstraight2_5[1][cstraight2_5_num[1]] && cstraight2_5[My_position][j] >= cstraight2_5[2][cstraight2_5_num[2]]
								&& cstraight2_5[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::STRAIGHT2,
									cstraight2_5[My_position][j], 5);
							}
						}
					}

					else
					{
						for (int j = 1; j <= cstraight2_5_num[My_position]; j++)
						{
							if (cstraight2_5[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::STRAIGHT2,
									cstraight2_5[My_position][j], 5);
							}
						}
					}
				}
			}
			else if (packs.size() == 6)
			{
				if (cstraight2_6[My_position][cstraight2_6_num[My_position]] > comboLevel)
				{

					if (cstraight2_6[My_position][cstraight2_6_num[My_position]] >= cstraight2_6[1][cstraight2_6_num[1]] &&
						cstraight2_6[My_position][cstraight2_6_num[My_position]] >= cstraight2_6[2][cstraight2_6_num[2]])
					{
						for (int j = 1; j <= cstraight2_6_num[My_position]; j++)
						{
							if (cstraight2_6[My_position][j] >= cstraight2_6[1][cstraight2_6_num[1]] && cstraight2_6[My_position][j] >= cstraight2_6[2][cstraight2_6_num[2]]
								&& cstraight2_6[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::STRAIGHT2,
									cstraight2_6[My_position][j], 6);
							}
						}
					}

					else
					{
						for (int j = 1; j <= cstraight2_6_num[My_position]; j++)
						{
							if (cstraight2_6[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::STRAIGHT2,
									cstraight2_6[My_position][j], 6);
							}
						}
					}
				}
			}
			if (cbomb_num[My_position] > 0)
			{
				return CardCombo(CardComboType::BOMB,
					cbomb[My_position][1], 1);
			}
			if (rocket[My_position] == 1)
			{
				return CardCombo(CardComboType::ROCKET);
			}
			return CardCombo(CardComboType::PASS);
		}
		else if (comboType == CardComboType::TRIPLET1)
		{
			if (csingle_num[My_position] > 0 && ctriplet_num[My_position] > 0)
			{
				if (ctriplet[My_position][ctriplet_num[My_position]] > comboLevel)
				{
					if (ctriplet[My_position][ctriplet_num[My_position]] >= ctriplet[1][ctriplet_num[1]] &&
						ctriplet[My_position][ctriplet_num[My_position]] >= ctriplet[2][ctriplet_num[2]])
					{
						for (int j = 1; j <= ctriplet_num[My_position]; j++)
						{
							if (ctriplet[My_position][j] >= ctriplet[1][ctriplet_num[1]] && ctriplet[My_position][j] >= ctriplet[2][ctriplet_num[2]]
								&& ctriplet[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::TRIPLET1,
									ctriplet[My_position][j], 2, csingle[My_position]);
							}
						}
					}
					else
					{
						for (int j = 1; j <= ctriplet_num[My_position]; j++)
						{
							if (ctriplet[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::TRIPLET1,
									ctriplet[My_position][j], 2, csingle[My_position]);
							}
						}
					}
				}
			}
			if (cbomb_num[My_position] > 0)
			{
				return CardCombo(CardComboType::BOMB,
					cbomb[My_position][1], 1);
			}
			if (rocket[My_position] == 1)
			{
				return CardCombo(CardComboType::ROCKET);
			}
			return CardCombo(CardComboType::PASS);
		}
		else if (comboType == CardComboType::TRIPLET2)
		{
			if (cpair_num[My_position] > 0 && ctriplet_num[My_position] > 0)
			{
				if (ctriplet[My_position][ctriplet_num[My_position]] > comboLevel)
				{
					if (ctriplet[My_position][ctriplet_num[My_position]] >= ctriplet[1][ctriplet_num[1]] &&
						ctriplet[My_position][ctriplet_num[My_position]] >= ctriplet[2][ctriplet_num[2]])
					{
						for (int j = 1; j <= ctriplet_num[My_position]; j++)
						{
							if (ctriplet[My_position][j] >= ctriplet[1][ctriplet_num[1]] && ctriplet[My_position][j] >= ctriplet[2][ctriplet_num[2]]
								&& ctriplet[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::TRIPLET2,
									ctriplet[My_position][j], 2, cpair[My_position]);
							}
						}
					}
					else
					{
						for (int j = 1; j <= ctriplet_num[My_position]; j++)
						{
							if (ctriplet[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::TRIPLET2,
									ctriplet[My_position][j], 2, cpair[My_position]);
							}
						}
					}
				}
			}
			if (cbomb_num[My_position] > 0)
			{
				return CardCombo(CardComboType::BOMB,
					cbomb[My_position][1], 1);
			}
			if (rocket[My_position] == 1)
			{
				return CardCombo(CardComboType::ROCKET);
			}
			return CardCombo(CardComboType::PASS);
		}
		else if (comboType == CardComboType::PLANE)
		{
			if (packs.size() == 2)
			{
				if (cstraight3_2[My_position][cstraight3_2_num[My_position]] > comboLevel)
				{

					if (cstraight3_2[My_position][cstraight3_2_num[My_position]] >= cstraight3_2[1][cstraight3_2_num[1]] &&
						cstraight3_2[My_position][cstraight3_2_num[My_position]] >= cstraight3_2[2][cstraight3_2_num[2]])
					{
						for (int j = 1; j <= cstraight3_2_num[My_position]; j++)
						{
							if (cstraight3_2[My_position][j] >= cstraight3_2[1][cstraight3_2_num[1]] && cstraight3_2[My_position][j] >= cstraight3_2[2][cstraight3_2_num[2]]
								&& cstraight3_2[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::PLANE,
									cstraight3_2[My_position][j], 2);
							}
						}
					}

					else
					{
						for (int j = 1; j <= cstraight3_2_num[My_position]; j++)
						{
							if (cstraight3_2[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::PLANE,
									cstraight3_2[My_position][j], 2);
							}
						}
					}
				}
			}
			else if (packs.size() == 3)
			{
				if (cstraight3_3[My_position][cstraight3_3_num[My_position]] > comboLevel)
				{

					if (cstraight3_3[My_position][cstraight3_3_num[My_position]] >= cstraight3_3[1][cstraight3_3_num[1]] &&
						cstraight3_3[My_position][cstraight3_3_num[My_position]] >= cstraight3_3[2][cstraight3_3_num[2]])
					{
						for (int j = 1; j <= cstraight3_3_num[My_position]; j++)
						{
							if (cstraight3_3[My_position][j] >= cstraight3_3[1][cstraight3_3_num[1]] && cstraight3_3[My_position][j] >= cstraight3_3[2][cstraight3_3_num[2]]
								&& cstraight3_3[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::PLANE,
									cstraight3_3[My_position][j], 3);
							}
						}
					}

					else
					{
						for (int j = 1; j <= cstraight3_3_num[My_position]; j++)
						{
							if (cstraight3_3[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::PLANE,
									cstraight3_3[My_position][j], 3);
							}
						}
					}
				}
			}
			else if (packs.size() == 4)
			{
				if (cstraight3_4[My_position][cstraight3_4_num[My_position]] > comboLevel)
				{

					if (cstraight3_4[My_position][cstraight3_4_num[My_position]] >= cstraight3_4[1][cstraight3_4_num[1]] &&
						cstraight3_4[My_position][cstraight3_4_num[My_position]] >= cstraight3_4[2][cstraight3_4_num[2]])
					{
						for (int j = 1; j <= cstraight3_4_num[My_position]; j++)
						{
							if (cstraight3_4[My_position][j] >= cstraight3_4[1][cstraight3_4_num[1]] && cstraight3_4[My_position][j] >= cstraight3_4[2][cstraight3_4_num[2]]
								&& cstraight3_4[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::PLANE,
									cstraight3_4[My_position][j], 4);
							}
						}
					}

					else
					{
						for (int j = 1; j <= cstraight3_4_num[My_position]; j++)
						{
							if (cstraight3_4[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::PLANE,
									cstraight3_4[My_position][j], 4);
							}
						}
					}
				}
			}
			if (cbomb_num[My_position] > 0)
			{
				return CardCombo(CardComboType::BOMB,
					cbomb[My_position][1], 1);
			}
			if (rocket[My_position] == 1)
			{
				return CardCombo(CardComboType::ROCKET);
			}
			return CardCombo(CardComboType::PASS);
		}
		else if (comboType == CardComboType::PLANE1)
		{
			if (packs.size() / 2 == 2)
			{
				if (cstraight3_2[My_position][cstraight3_2_num[My_position]] > comboLevel && csingle_num[My_position] > packs.size() / 2)
				{

					if (cstraight3_2[My_position][cstraight3_2_num[My_position]] >= cstraight3_2[1][cstraight3_2_num[1]] &&
						cstraight3_2[My_position][cstraight3_2_num[My_position]] >= cstraight3_2[2][cstraight3_2_num[2]])
					{
						for (int j = 1; j <= cstraight3_2_num[My_position]; j++)
						{
							if (cstraight3_2[My_position][j] >= cstraight3_2[1][cstraight3_2_num[1]] && cstraight3_2[My_position][j] >= cstraight3_2[2][cstraight3_2_num[2]]
								&& cstraight3_2[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::PLANE1,
									cstraight3_2[My_position][j], 2, csingle[My_position]);
							}
						}
					}

					else
					{
						for (int j = 1; j <= cstraight3_2_num[My_position]; j++)
						{
							if (cstraight3_2[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::PLANE1,
									cstraight3_2[My_position][j], 2, csingle[My_position]);
							}
						}
					}
				}
			}
			else if (packs.size() / 2 == 3)
			{
				if (cstraight3_3[My_position][cstraight3_3_num[My_position]] > comboLevel && csingle_num[My_position] > packs.size() / 2)
				{

					if (cstraight3_3[My_position][cstraight3_3_num[My_position]] >= cstraight3_3[1][cstraight3_3_num[1]] &&
						cstraight3_3[My_position][cstraight3_3_num[My_position]] >= cstraight3_3[2][cstraight3_3_num[2]])
					{
						for (int j = 1; j <= cstraight3_3_num[My_position]; j++)
						{
							if (cstraight3_3[My_position][j] >= cstraight3_3[1][cstraight3_3_num[1]] && cstraight3_3[My_position][j] >= cstraight3_3[2][cstraight3_3_num[2]]
								&& cstraight3_3[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::PLANE1,
									cstraight3_3[My_position][j], 3, csingle[My_position]);
							}
						}
					}

					else
					{
						for (int j = 1; j <= cstraight3_3_num[My_position]; j++)
						{
							if (cstraight3_3[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::PLANE1,
									cstraight3_3[My_position][j], 3, csingle[My_position]);
							}
						}
					}
				}
			}
			else if (packs.size() / 2 == 4)
			{
				if (cstraight3_4[My_position][cstraight3_4_num[My_position]] > comboLevel && csingle_num[My_position] > packs.size() / 2)
				{

					if (cstraight3_4[My_position][cstraight3_4_num[My_position]] >= cstraight3_4[1][cstraight3_4_num[1]] &&
						cstraight3_4[My_position][cstraight3_4_num[My_position]] >= cstraight3_4[2][cstraight3_4_num[2]])
					{
						for (int j = 1; j <= cstraight3_4_num[My_position]; j++)
						{
							if (cstraight3_4[My_position][j] >= cstraight3_4[1][cstraight3_4_num[1]] && cstraight3_4[My_position][j] >= cstraight3_4[2][cstraight3_4_num[2]]
								&& cstraight3_4[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::PLANE1,
									cstraight3_4[My_position][j], 4, csingle[My_position]);
							}
						}
					}

					else
					{
						for (int j = 1; j <= cstraight3_4_num[My_position]; j++)
						{
							if (cstraight3_4[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::PLANE1,
									cstraight3_4[My_position][j], 4, csingle[My_position]);
							}
						}
					}
				}
			}
			if (cbomb_num[My_position] > 0)
			{
				return CardCombo(CardComboType::BOMB,
					cbomb[My_position][1], 1);
			}
			if (rocket[My_position] == 1)
			{
				return CardCombo(CardComboType::ROCKET);
			}
			return CardCombo(CardComboType::PASS);
		}
		else if (comboType == CardComboType::PLANE2)
		{
			if (packs.size() / 2 == 2)
			{
				if (cstraight3_2[My_position][cstraight3_2_num[My_position]] > comboLevel && cpair_num[My_position] > packs.size() / 2)
				{

					if (cstraight3_2[My_position][cstraight3_2_num[My_position]] >= cstraight3_2[1][cstraight3_2_num[1]] &&
						cstraight3_2[My_position][cstraight3_2_num[My_position]] >= cstraight3_2[2][cstraight3_2_num[2]])
					{
						for (int j = 1; j <= cstraight3_2_num[My_position]; j++)
						{
							if (cstraight3_2[My_position][j] >= cstraight3_2[1][cstraight3_2_num[1]] && cstraight3_2[My_position][j] >= cstraight3_2[2][cstraight3_2_num[2]]
								&& cstraight3_2[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::PLANE2,
									cstraight3_2[My_position][j], 2, cpair[My_position]);
							}
						}
					}

					else
					{
						for (int j = 1; j <= cstraight3_2_num[My_position]; j++)
						{
							if (cstraight3_2[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::PLANE2,
									cstraight3_2[My_position][j], 2, cpair[My_position]);
							}
						}
					}
				}
			}
			else if (packs.size() / 2 == 3)
			{
				if (cstraight3_3[My_position][cstraight3_3_num[My_position]] > comboLevel && cpair_num[My_position] > packs.size() / 2)
				{

					if (cstraight3_3[My_position][cstraight3_3_num[My_position]] >= cstraight3_3[1][cstraight3_3_num[1]] &&
						cstraight3_3[My_position][cstraight3_3_num[My_position]] >= cstraight3_3[2][cstraight3_3_num[2]])
					{
						for (int j = 1; j <= cstraight3_3_num[My_position]; j++)
						{
							if (cstraight3_3[My_position][j] >= cstraight3_3[1][cstraight3_3_num[1]] && cstraight3_3[My_position][j] >= cstraight3_3[2][cstraight3_3_num[2]]
								&& cstraight3_3[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::PLANE2,
									cstraight3_3[My_position][j], 3, cpair[My_position]);
							}
						}
					}

					else
					{
						for (int j = 1; j <= cstraight3_3_num[My_position]; j++)
						{
							if (cstraight3_3[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::PLANE2,
									cstraight3_3[My_position][j], 3, cpair[My_position]);
							}
						}
					}
				}
			}
			else if (packs.size() / 2 == 4)
			{
				if (cstraight3_4[My_position][cstraight3_4_num[My_position]] > comboLevel && cpair_num[My_position] > packs.size() / 2)
				{

					if (cstraight3_4[My_position][cstraight3_4_num[My_position]] >= cstraight3_4[1][cstraight3_4_num[1]] &&
						cstraight3_4[My_position][cstraight3_4_num[My_position]] >= cstraight3_4[2][cstraight3_4_num[2]])
					{
						for (int j = 1; j <= cstraight3_4_num[My_position]; j++)
						{
							if (cstraight3_4[My_position][j] >= cstraight3_4[1][cstraight3_4_num[1]] && cstraight3_4[My_position][j] >= cstraight3_4[2][cstraight3_4_num[2]]
								&& cstraight3_4[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::PLANE2,
									cstraight3_4[My_position][j], 4);
							}
						}
					}

					else
					{
						for (int j = 1; j <= cstraight3_4_num[My_position]; j++)
						{
							if (cstraight3_4[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::PLANE2,
									cstraight3_4[My_position][j], 4);
							}
						}
					}
				}
			}
			if (cbomb_num[My_position] > 0)
			{
				return CardCombo(CardComboType::BOMB,
					cbomb[My_position][1], 1);
			}
			if (rocket[My_position] == 1)
			{
				return CardCombo(CardComboType::ROCKET);
			}
			return CardCombo(CardComboType::PASS);
		}
		else if (comboType == CardComboType::QUADRUPLE2)
		{
			if (cbomb_num[My_position] > 0)
			{
				return CardCombo(CardComboType::BOMB,
					cbomb[My_position][1], 1);
			}
			return CardCombo(CardComboType::PASS);
		}
		else if (comboType == CardComboType::QUADRUPLE4)
		{
			if (cbomb_num[My_position] > 0)
			{
				return CardCombo(CardComboType::BOMB,
					cbomb[My_position][1], 1);
			}
			return CardCombo(CardComboType::PASS);
		}
		else if (comboType == CardComboType::ROCKET)
		{
			return CardCombo(CardComboType::PASS);
		}

	}
	else if (My_position == 1)//农民甲
	{
		if (this_combo_from == 0)
		{
			if (comboType == CardComboType::PASS)
			{
				if (cstraight2_6_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT2,
						cstraight2_6[My_position][cstraight2_6_num[My_position]], 6);
				}
				if (cstraight2_5_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT2,
						cstraight2_5[My_position][cstraight2_5_num[My_position]], 5);
				}
				if (cstraight2_4_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT2,
						cstraight2_4[My_position][cstraight2_4_num[My_position]], 4);
				}
				if (cstraight2_3_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT2,
						cstraight2_3[My_position][cstraight2_3_num[My_position]], 3);
				}
				if (cstraight_9_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT,
						cstraight_9[My_position][cstraight_9_num[My_position]], 9);
				}
				if (cstraight_8_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT,
						cstraight_8[My_position][cstraight_8_num[My_position]], 8);
				}
				if (cstraight_7_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT,
						cstraight_7[My_position][cstraight_7_num[My_position]], 7);
				}
				if (cstraight_6_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT,
						cstraight_6[My_position][cstraight_6_num[My_position]], 6);
				}
				if (cstraight_5_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT,
						cstraight_5[My_position][cstraight_5_num[My_position]], 5);
				}
				if (cstraight3_4_num[My_position] > 0)
				{
					if (csingle_num[My_position] >= 4)
					{
						return CardCombo(CardComboType::PLANE1,
							cstraight3_4[My_position][cstraight3_4_num[My_position]], 4, csingle[My_position]);
					}
					if (cpair_num[My_position] >= 4)
					{
						return CardCombo(CardComboType::PLANE2,
							cstraight3_4[My_position][cstraight3_4_num[My_position]], 4, cpair[My_position]);
					}
				}
				if (cstraight3_3_num[My_position] > 0)
				{
					if (csingle_num[My_position] >= 3)
					{
						return CardCombo(CardComboType::PLANE1,
							cstraight3_3[My_position][cstraight3_3_num[My_position]], 3, csingle[My_position]);
					}
					else if (cpair_num[My_position] >= 3)
					{
						return CardCombo(CardComboType::PLANE2,
							cstraight3_3[My_position][cstraight3_3_num[My_position]], 3, cpair[My_position]);
					}
				}
				if (cstraight3_2_num[My_position] > 0)
				{
					if (csingle_num[My_position] >= 2)
					{
						return CardCombo(CardComboType::PLANE1,
							cstraight3_2[My_position][cstraight3_2_num[My_position]], 2, csingle[My_position]);
					}
					if (cpair_num[My_position] >= 2)
					{
						return CardCombo(CardComboType::PLANE2,
							cstraight3_2[My_position][cstraight3_2_num[My_position]], 2, cpair[My_position]);
					}
				}
				if (ctriplet_num[My_position > 0])
				{
					if (csingle_num[My_position] > 0)
					{
						cout << csingle[My_position][0] << endl;
						cout << endl << endl;
						return CardCombo(CardComboType::TRIPLET1,
							ctriplet[My_position][1], 2, csingle[My_position]);
					}
					if (cpair_num[My_position] > 0)
					{
						return CardCombo(CardComboType::TRIPLET1,
							ctriplet[My_position][1], 2, cpair[My_position]);
					}
					return CardCombo(CardComboType::TRIPLET,
						ctriplet[My_position][1], 1);
				}
				if (cstraight3_4_num[My_position] > 0)
				{
					return CardCombo(CardComboType::PLANE,
						cstraight3_4[My_position][cstraight3_4_num[My_position]], 4);
				}
				if (cstraight3_3_num[My_position] > 0)
				{
					return CardCombo(CardComboType::PLANE,
						cstraight3_3[My_position][cstraight3_3_num[My_position]], 3);
				}
				if (cstraight3_2_num[My_position] > 0)
				{
					return CardCombo(CardComboType::PLANE,
						cstraight3_2[My_position][cstraight3_2_num[My_position]], 2);
				}
				if (cpair_num[My_position] != 0)
				{
					return CardCombo(CardComboType::PAIR,
						cpair[My_position][1], 1);
				}
				if (csingle_num[My_position] != 0)
				{
					return CardCombo(CardComboType::SINGLE,
						csingle[My_position][1], 1);
				}
				if (cbomb_num[My_position] != 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				if (rocket[My_position] != 0)
				{
					return CardCombo(CardComboType::ROCKET,
						15, 1);
				}
			}
			else if (comboType == CardComboType::SINGLE)
			{
				if (csingle[My_position][csingle_num[My_position]] > comboLevel)
				{
					if (csingle[My_position][csingle_num[My_position]] >= csingle[0][csingle_num[0]])
					{
						for (int j = 1; j <= csingle_num[My_position]; j++)
						{
							if (csingle[My_position][j] >= csingle[0][csingle_num[0]] && csingle[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::SINGLE,
									csingle[My_position][j], 1);
							}
						}
					}
					else
					{
						for (int j = 1; j <= csingle_num[My_position]; j++)
						{
							if (csingle[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::SINGLE,
									csingle[My_position][j], 1);
							}
						}
					}
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::PAIR)
			{
				if (cpair[My_position][cpair_num[My_position]] > comboLevel)
				{

					if (cpair[My_position][cpair_num[My_position]] >= cpair[0][cpair_num[0]])
					{
						for (int j = 1; j <= cpair_num[My_position]; j++)
						{
							if (cpair[My_position][j] >= cpair[0][cpair_num[0]]
								&& cpair[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::PAIR,
									cpair[My_position][j], 1);
							}
						}
					}

					else
					{
						for (int j = 1; j <= cpair_num[My_position]; j++)
						{
							if (cpair[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::PAIR,
									cpair[My_position][j], 1);
							}
						}
					}
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::TRIPLET)
			{
				if (ctriplet[My_position][ctriplet_num[My_position]] > comboLevel)
				{

					if (ctriplet[My_position][ctriplet_num[My_position]] >= ctriplet[0][ctriplet_num[0]])
					{
						for (int j = 1; j <= ctriplet_num[My_position]; j++)
						{
							if (ctriplet[My_position][j] >= ctriplet[0][ctriplet_num[0]] &&
								ctriplet[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::TRIPLET,
									ctriplet[My_position][j], 1);
							}
						}
					}

					else
					{
						for (int j = 1; j <= ctriplet_num[My_position]; j++)
						{
							if (ctriplet[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::TRIPLET,
									ctriplet[My_position][j], 1);
							}
						}
					}
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::BOMB)
			{
				if (cbomb[My_position][cbomb_num[My_position]] > comboLevel)
				{

					if (cbomb[My_position][cbomb_num[My_position]] >= cbomb[0][cbomb_num[0]])
					{
						for (int j = 1; j <= cbomb_num[My_position]; j++)
						{
							if (cbomb[My_position][j] >= cbomb[0][cbomb_num[0]]
								&& cbomb[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::BOMB,
									cbomb[My_position][j], 1);
							}
						}

					}
					else
					{
						for (int j = 1; j <= cbomb_num[My_position]; j++)
						{
							if (cbomb[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::BOMB,
									cbomb[My_position][j], 1);
							}
						}
					}
				}
				if (rocket[My_position] == 1)
				{
					return CardCombo(CardComboType::ROCKET);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::STRAIGHT)
			{
				if (packs.size() == 5)
				{
					if (cstraight_5[My_position][cstraight_5_num[My_position]] > comboLevel)
					{

						if (cstraight_5[My_position][cstraight_5_num[My_position]] >= cstraight_5[0][cstraight_5_num[0]])
						{
							for (int j = 1; j <= cstraight_5_num[My_position]; j++)
							{
								if (cstraight_5[My_position][j] >= cstraight_5[0][cstraight_5_num[0]]
									&& cstraight_5[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_5[My_position][j], 5);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight_5_num[My_position]; j++)
							{
								if (cstraight_5[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_5[My_position][j], 5);
								}
							}
						}
					}
				}
				else if (packs.size() == 6)
				{
					if (cstraight_6[My_position][cstraight_6_num[My_position]] > comboLevel)
					{

						if (cstraight_6[My_position][cstraight_6_num[My_position]] >= cstraight_6[0][cstraight_6_num[0]])
						{
							for (int j = 1; j <= cstraight_6_num[My_position]; j++)
							{
								if (cstraight_6[My_position][j] >= cstraight_6[0][cstraight_6_num[0]] && cstraight_6[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_6[My_position][j], 6);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight_6_num[My_position]; j++)
							{
								if (cstraight_6[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_6[My_position][j], 6);
								}
							}
						}
					}
				}
				else if (packs.size() == 7)
				{
					if (cstraight_7[My_position][cstraight_7_num[My_position]] > comboLevel)
					{

						if (cstraight_7[My_position][cstraight_7_num[My_position]] >= cstraight_7[0][cstraight_7_num[0]])
						{
							for (int j = 1; j <= cstraight_7_num[My_position]; j++)
							{
								if (cstraight_7[My_position][j] >= cstraight_7[0][cstraight_7_num[0]] && cstraight_7[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_7[My_position][j], 7);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight_7_num[My_position]; j++)
							{
								if (cstraight_7[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_7[My_position][j], 7);
								}
							}
						}
					}
				}
				else if (packs.size() == 8)
				{
					if (cstraight_8[My_position][cstraight_8_num[My_position]] > comboLevel)
					{

						if (cstraight_8[My_position][cstraight_8_num[My_position]] >= cstraight_8[0][cstraight_8_num[0]])
						{
							for (int j = 1; j <= cstraight_8_num[My_position]; j++)
							{
								if (cstraight_8[My_position][j] >= cstraight_8[0][cstraight_8_num[0]]
									&& cstraight_8[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_8[My_position][j], 8);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight_8_num[My_position]; j++)
							{
								if (cstraight_8[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_8[My_position][j], 8);
								}
							}
						}
					}
				}
				else if (packs.size() == 9)
				{
					if (cstraight_9[My_position][cstraight_9_num[My_position]] > comboLevel)
					{

						if (cstraight_9[My_position][cstraight_9_num[My_position]] >= cstraight_9[0][cstraight_9_num[0]])
						{
							for (int j = 1; j <= cstraight_9_num[My_position]; j++)
							{
								if (cstraight_9[My_position][j] >= cstraight_9[0][cstraight_9_num[0]]
									&& cstraight_9[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_9[My_position][j], 9);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight_9_num[My_position]; j++)
							{
								if (cstraight_9[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_9[My_position][j], 9);
								}
							}
						}
					}
				}
				if (cbomb_num[My_position] > 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				if (rocket[My_position] == 1)
				{
					return CardCombo(CardComboType::ROCKET);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::STRAIGHT2)
			{
				if (packs.size() == 3)
				{
					if (cstraight2_3[My_position][cstraight2_3_num[My_position]] > comboLevel)
					{

						if (cstraight2_3[My_position][cstraight2_3_num[My_position]] >= cstraight2_3[0][cstraight2_3_num[0]])
						{
							for (int j = 1; j <= cstraight2_3_num[My_position]; j++)
							{
								if (cstraight2_3[My_position][j] >= cstraight2_3[0][cstraight2_3_num[0]]
									&& cstraight2_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_3[My_position][j], 3);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight2_3_num[My_position]; j++)
							{
								if (cstraight2_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_3[My_position][j], 3);
								}
							}
						}
					}
				}
				else if (packs.size() == 4)
				{
					if (cstraight2_4[My_position][cstraight2_4_num[My_position]] > comboLevel)
					{

						if (cstraight2_4[My_position][cstraight2_4_num[My_position]] >= cstraight2_4[0][cstraight2_4_num[0]])
						{
							for (int j = 1; j <= cstraight2_4_num[My_position]; j++)
							{
								if (cstraight2_4[My_position][j] >= cstraight2_4[0][cstraight2_4_num[0]] && cstraight2_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_4[My_position][j], 4);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight2_4_num[My_position]; j++)
							{
								if (cstraight2_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_4[My_position][j], 4);
								}
							}
						}
					}
				}
				else if (packs.size() == 5)
				{
					if (cstraight2_5[My_position][cstraight2_5_num[My_position]] > comboLevel)
					{

						if (cstraight2_5[My_position][cstraight2_5_num[My_position]] >= cstraight2_5[0][cstraight2_5_num[0]])
						{
							for (int j = 1; j <= cstraight2_5_num[My_position]; j++)
							{
								if (cstraight2_5[My_position][j] >= cstraight2_5[0][cstraight2_5_num[0]] && cstraight2_5[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_5[My_position][j], 5);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight2_5_num[My_position]; j++)
							{
								if (cstraight2_5[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_5[My_position][j], 5);
								}
							}
						}
					}
				}
				else if (packs.size() == 6)
				{
					if (cstraight2_6[My_position][cstraight2_6_num[My_position]] > comboLevel)
					{

						if (cstraight2_6[My_position][cstraight2_6_num[My_position]] >= cstraight2_6[0][cstraight2_6_num[0]])
						{
							for (int j = 1; j <= cstraight2_6_num[My_position]; j++)
							{
								if (cstraight2_6[My_position][j] >= cstraight2_6[0][cstraight2_6_num[0]] && cstraight2_6[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_6[My_position][j], 6);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight2_6_num[My_position]; j++)
							{
								if (cstraight2_6[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_6[My_position][j], 6);
								}
							}
						}
					}
				}
				if (cbomb_num[My_position] > 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				if (rocket[My_position] == 1)
				{
					return CardCombo(CardComboType::ROCKET);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::TRIPLET1)
			{
				if (csingle_num[My_position] > 0 && ctriplet_num[My_position] > 0)
				{
					if (ctriplet[My_position][ctriplet_num[My_position]] > comboLevel)
					{
						if (ctriplet[My_position][ctriplet_num[My_position]] >= ctriplet[0][ctriplet_num[0]])
						{
							for (int j = 1; j <= ctriplet_num[My_position]; j++)
							{
								if (ctriplet[My_position][j] >= ctriplet[0][ctriplet_num[0]] && ctriplet[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::TRIPLET1,
										ctriplet[My_position][j], 2, csingle[My_position]);
								}
							}
						}
						else
						{
							for (int j = 1; j <= ctriplet_num[My_position]; j++)
							{
								if (ctriplet[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::TRIPLET1,
										ctriplet[My_position][j], 2, csingle[My_position]);
								}
							}
						}
					}
				}
				if (cbomb_num[My_position] > 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				if (rocket[My_position] == 1)
				{
					return CardCombo(CardComboType::ROCKET);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::TRIPLET2)
			{
				if (cpair_num[My_position] > 0 && ctriplet_num[My_position] > 0)
				{
					if (ctriplet[My_position][ctriplet_num[My_position]] > comboLevel)
					{
						if (ctriplet[My_position][ctriplet_num[My_position]] >= ctriplet[0][ctriplet_num[0]])
						{
							for (int j = 1; j <= ctriplet_num[My_position]; j++)
							{
								if (ctriplet[My_position][j] >= ctriplet[0][ctriplet_num[0]] && ctriplet[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::TRIPLET2,
										ctriplet[My_position][j], 2, cpair[My_position]);
								}
							}
						}
						else
						{
							for (int j = 1; j <= ctriplet_num[My_position]; j++)
							{
								if (ctriplet[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::TRIPLET2,
										ctriplet[My_position][j], 2, cpair[My_position]);
								}
							}
						}
					}
				}
				if (cbomb_num[My_position] > 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				if (rocket[My_position] == 1)
				{
					return CardCombo(CardComboType::ROCKET);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::PLANE)
			{
				if (packs.size() == 2)
				{
					if (cstraight3_2[My_position][cstraight3_2_num[My_position]] > comboLevel)
					{

						if (cstraight3_2[My_position][cstraight3_2_num[My_position]] >= cstraight3_2[0][cstraight3_2_num[0]])
						{
							for (int j = 1; j <= cstraight3_2_num[My_position]; j++)
							{
								if (cstraight3_2[My_position][j] >= cstraight3_2[0][cstraight3_2_num[0]] && cstraight3_2[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE,
										cstraight3_2[My_position][j], 2);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight3_2_num[My_position]; j++)
							{
								if (cstraight3_2[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE,
										cstraight3_2[My_position][j], 2);
								}
							}
						}
					}
				}
				else if (packs.size() == 3)
				{
					if (cstraight3_3[My_position][cstraight3_3_num[My_position]] > comboLevel)
					{

						if (cstraight3_3[My_position][cstraight3_3_num[My_position]] >= cstraight3_3[0][cstraight3_3_num[0]])
						{
							for (int j = 1; j <= cstraight3_3_num[My_position]; j++)
							{
								if (cstraight3_3[My_position][j] >= cstraight3_3[0][cstraight3_3_num[0]] && cstraight3_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE,
										cstraight3_3[My_position][j], 3);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight3_3_num[My_position]; j++)
							{
								if (cstraight3_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE,
										cstraight3_3[My_position][j], 3);
								}
							}
						}
					}
				}
				else if (packs.size() == 4)
				{
					if (cstraight3_4[My_position][cstraight3_4_num[My_position]] > comboLevel)
					{

						if (cstraight3_4[My_position][cstraight3_4_num[My_position]] >= cstraight3_4[0][cstraight3_4_num[0]])
						{
							for (int j = 1; j <= cstraight3_4_num[My_position]; j++)
							{
								if (cstraight3_4[My_position][j] >= cstraight3_4[0][cstraight3_4_num[0]] && cstraight3_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE,
										cstraight3_4[My_position][j], 4);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight3_4_num[My_position]; j++)
							{
								if (cstraight3_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE,
										cstraight3_4[My_position][j], 4);
								}
							}
						}
					}
				}
				if (cbomb_num[My_position] > 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				if (rocket[My_position] == 1)
				{
					return CardCombo(CardComboType::ROCKET);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::PLANE1)
			{
				if (packs.size() / 2 == 2)
				{
					if (cstraight3_2[My_position][cstraight3_2_num[My_position]] > comboLevel && csingle_num[My_position] > packs.size() / 2)
					{

						if (cstraight3_2[My_position][cstraight3_2_num[My_position]] >= cstraight3_2[0][cstraight3_2_num[0]])
						{
							for (int j = 1; j <= cstraight3_2_num[My_position]; j++)
							{
								if (cstraight3_2[My_position][j] >= cstraight3_2[0][cstraight3_2_num[0]] && cstraight3_2[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE1,
										cstraight3_2[My_position][j], 2, csingle[My_position]);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight3_2_num[My_position]; j++)
							{
								if (cstraight3_2[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE1,
										cstraight3_2[My_position][j], 2, csingle[My_position]);
								}
							}
						}
					}
				}
				else if (packs.size() / 2 == 3)
				{
					if (cstraight3_3[My_position][cstraight3_3_num[My_position]] > comboLevel && csingle_num[My_position] > packs.size() / 2)
					{

						if (cstraight3_3[My_position][cstraight3_3_num[My_position]] >= cstraight3_3[0][cstraight3_3_num[0]])
						{
							for (int j = 1; j <= cstraight3_3_num[My_position]; j++)
							{
								if (cstraight3_3[My_position][j] >= cstraight3_3[0][cstraight3_3_num[0]] && cstraight3_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE1,
										cstraight3_3[My_position][j], 3, csingle[My_position]);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight3_3_num[My_position]; j++)
							{
								if (cstraight3_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE1,
										cstraight3_3[My_position][j], 3, csingle[My_position]);
								}
							}
						}
					}
				}
				else if (packs.size() / 2 == 4)
				{
					if (cstraight3_4[My_position][cstraight3_4_num[My_position]] > comboLevel && csingle_num[My_position] > packs.size() / 2)
					{

						if (cstraight3_4[My_position][cstraight3_4_num[My_position]] >= cstraight3_4[0][cstraight3_4_num[0]])
						{
							for (int j = 1; j <= cstraight3_4_num[My_position]; j++)
							{
								if (cstraight3_4[My_position][j] >= cstraight3_4[0][cstraight3_4_num[0]] && cstraight3_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE1,
										cstraight3_4[My_position][j], 4, csingle[My_position]);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight3_4_num[My_position]; j++)
							{
								if (cstraight3_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE1,
										cstraight3_4[My_position][j], 4, csingle[My_position]);
								}
							}
						}
					}
				}
				if (cbomb_num[My_position] > 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				if (rocket[My_position] == 1)
				{
					return CardCombo(CardComboType::ROCKET);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::PLANE2)
			{
				if (packs.size() / 2 == 2)
				{
					if (cstraight3_2[My_position][cstraight3_2_num[My_position]] > comboLevel && cpair_num[My_position] > packs.size() / 2)
					{

						if (cstraight3_2[My_position][cstraight3_2_num[My_position]] >= cstraight3_2[0][cstraight3_2_num[0]])
						{
							for (int j = 1; j <= cstraight3_2_num[My_position]; j++)
							{
								if (cstraight3_2[My_position][j] >= cstraight3_2[0][cstraight3_2_num[0]] && cstraight3_2[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE2,
										cstraight3_2[My_position][j], 2, cpair[My_position]);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight3_2_num[My_position]; j++)
							{
								if (cstraight3_2[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE2,
										cstraight3_2[My_position][j], 2, cpair[My_position]);
								}
							}
						}
					}
				}
				else if (packs.size() / 2 == 3)
				{
					if (cstraight3_3[My_position][cstraight3_3_num[My_position]] > comboLevel && cpair_num[My_position] > packs.size() / 2)
					{

						if (cstraight3_3[My_position][cstraight3_3_num[My_position]] >= cstraight3_3[0][cstraight3_3_num[0]])
						{
							for (int j = 1; j <= cstraight3_3_num[My_position]; j++)
							{
								if (cstraight3_3[My_position][j] >= cstraight3_3[0][cstraight3_3_num[0]] && cstraight3_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE2,
										cstraight3_3[My_position][j], 3, cpair[My_position]);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight3_3_num[My_position]; j++)
							{
								if (cstraight3_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE2,
										cstraight3_3[My_position][j], 3, cpair[My_position]);
								}
							}
						}
					}
				}
				else if (packs.size() / 2 == 4)
				{
					if (cstraight3_4[My_position][cstraight3_4_num[My_position]] > comboLevel && cpair_num[My_position] > packs.size() / 2)
					{

						if (cstraight3_4[My_position][cstraight3_4_num[My_position]] >= cstraight3_4[0][cstraight3_4_num[0]])
						{
							for (int j = 1; j <= cstraight3_4_num[My_position]; j++)
							{
								if (cstraight3_4[My_position][j] >= cstraight3_4[0][cstraight3_4_num[0]] && cstraight3_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE2,
										cstraight3_4[My_position][j], 4);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight3_4_num[My_position]; j++)
							{
								if (cstraight3_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE2,
										cstraight3_4[My_position][j], 4);
								}
							}
						}
					}
				}
				if (cbomb_num[My_position] > 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				if (rocket[My_position] == 1)
				{
					return CardCombo(CardComboType::ROCKET);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::QUADRUPLE2)
			{
				if (cbomb_num[My_position] > 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::QUADRUPLE4)
			{
				if (cbomb_num[My_position] > 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::ROCKET)
			{
				return CardCombo(CardComboType::PASS);
			}
		}
		if (this_combo_from == 2)
		{
			if (comboType == CardComboType::PASS)
			{
				if (cstraight2_6_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT2,
						cstraight2_6[My_position][cstraight2_6_num[My_position]], 6);
				}
				if (cstraight2_5_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT2,
						cstraight2_5[My_position][cstraight2_5_num[My_position]], 5);
				}
				if (cstraight2_4_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT2,
						cstraight2_4[My_position][cstraight2_4_num[My_position]], 4);
				}
				if (cstraight2_3_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT2,
						cstraight2_3[My_position][cstraight2_3_num[My_position]], 3);
				}
				if (cstraight_9_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT,
						cstraight_9[My_position][cstraight_9_num[My_position]], 9);
				}
				if (cstraight_8_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT,
						cstraight_8[My_position][cstraight_8_num[My_position]], 8);
				}
				if (cstraight_7_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT,
						cstraight_7[My_position][cstraight_7_num[My_position]], 7);
				}
				if (cstraight_6_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT,
						cstraight_6[My_position][cstraight_6_num[My_position]], 6);
				}
				if (cstraight_5_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT,
						cstraight_5[My_position][cstraight_5_num[My_position]], 5);
				}
				if (cstraight3_4_num[My_position] > 0)
				{
					if (csingle_num[My_position] >= 4)
					{
						return CardCombo(CardComboType::PLANE1,
							cstraight3_4[My_position][cstraight3_4_num[My_position]], 4, csingle[My_position]);
					}
					if (cpair_num[My_position] >= 4)
					{
						return CardCombo(CardComboType::PLANE2,
							cstraight3_4[My_position][cstraight3_4_num[My_position]], 4, cpair[My_position]);
					}
				}
				if (cstraight3_3_num[My_position] > 0)
				{
					if (csingle_num[My_position] >= 3)
					{
						return CardCombo(CardComboType::PLANE1,
							cstraight3_3[My_position][cstraight3_3_num[My_position]], 3, csingle[My_position]);
					}
					else if (cpair_num[My_position] >= 3)
					{
						return CardCombo(CardComboType::PLANE2,
							cstraight3_3[My_position][cstraight3_3_num[My_position]], 3, cpair[My_position]);
					}
				}
				if (cstraight3_2_num[My_position] > 0)
				{
					if (csingle_num[My_position] >= 2)
					{
						return CardCombo(CardComboType::PLANE1,
							cstraight3_2[My_position][cstraight3_2_num[My_position]], 2, csingle[My_position]);
					}
					if (cpair_num[My_position] >= 2)
					{
						return CardCombo(CardComboType::PLANE2,
							cstraight3_2[My_position][cstraight3_2_num[My_position]], 2, cpair[My_position]);
					}
				}
				if (ctriplet_num[My_position] > 0)
				{
					if (csingle_num[My_position] > 0)
					{
						return CardCombo(CardComboType::TRIPLET1,
							ctriplet[My_position][1], 2, csingle[My_position]);
					}
					if (cpair_num[My_position] > 0)
					{
						return CardCombo(CardComboType::TRIPLET1,
							ctriplet[My_position][1], 2, cpair[My_position]);
					}
					return CardCombo(CardComboType::TRIPLET,
						ctriplet[My_position][1], 1);
				}
				if (cstraight3_4_num[My_position] > 0)
				{
					return CardCombo(CardComboType::PLANE,
						cstraight3_4[My_position][cstraight3_4_num[My_position]], 4);
				}
				if (cstraight3_3_num[My_position] > 0)
				{
					return CardCombo(CardComboType::PLANE,
						cstraight3_3[My_position][cstraight3_3_num[My_position]], 3);
				}
				if (cstraight3_2_num[My_position] > 0)
				{
					return CardCombo(CardComboType::PLANE,
						cstraight3_2[My_position][cstraight3_2_num[My_position]], 2);
				}
				if (cpair_num[My_position] != 0)
				{
					return CardCombo(CardComboType::PAIR,
						cpair[My_position][1], 1);
				}
				if (csingle_num[My_position] != 0)
				{
					return CardCombo(CardComboType::SINGLE,
						csingle[My_position][1], 1);
				}
				if (cbomb_num[My_position] != 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				if (rocket[My_position] != 0)
				{
					return CardCombo(CardComboType::ROCKET,
						15, 1);
				}
			}
			else if (comboType == CardComboType::SINGLE)
			{
				if (csingle[My_position][csingle_num[My_position]] > comboLevel)
				{
					{
						for (int j = 1; j <= csingle_num[My_position]; j++)
						{
							if (csingle[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::SINGLE,
									csingle[My_position][j], 1);
							}
						}
					}
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::PAIR)
			{
				if (cpair[My_position][cpair_num[My_position]] > comboLevel)
				{
					{
						for (int j = 1; j <= cpair_num[My_position]; j++)
						{
							if (cpair[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::PAIR,
									cpair[My_position][j], 1);
							}
						}
					}
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::TRIPLET)
			{
				if (ctriplet[My_position][ctriplet_num[My_position]] > comboLevel)
				{
					{
						for (int j = 1; j <= ctriplet_num[My_position]; j++)
						{
							if (ctriplet[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::TRIPLET,
									ctriplet[My_position][j], 1);
							}
						}
					}
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::BOMB)
			{
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::STRAIGHT)
			{
				if (packs.size() == 5)
				{
					if (cstraight_5[My_position][cstraight_5_num[My_position]] > comboLevel)
					{
						{
							for (int j = 1; j <= cstraight_5_num[My_position]; j++)
							{
								if (cstraight_5[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_5[My_position][j], 5);
								}
							}
						}
					}
				}
				else if (packs.size() == 6)
				{
					if (cstraight_6[My_position][cstraight_6_num[My_position]] > comboLevel)
					{
						{
							for (int j = 1; j <= cstraight_6_num[My_position]; j++)
							{
								if (cstraight_6[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_6[My_position][j], 6);
								}
							}
						}
					}
				}
				else if (packs.size() == 7)
				{
					if (cstraight_7[My_position][cstraight_7_num[My_position]] > comboLevel)
					{
						{
							for (int j = 1; j <= cstraight_7_num[My_position]; j++)
							{
								if (cstraight_7[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_7[My_position][j], 7);
								}
							}
						}
					}
				}
				else if (packs.size() == 8)
				{
					if (cstraight_8[My_position][cstraight_8_num[My_position]] > comboLevel)
					{
						{
							for (int j = 1; j <= cstraight_8_num[My_position]; j++)
							{
								if (cstraight_8[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_8[My_position][j], 8);
								}
							}
						}
					}
				}
				else if (packs.size() == 9)
				{
					if (cstraight_9[My_position][cstraight_9_num[My_position]] > comboLevel)
					{
						{
							for (int j = 1; j <= cstraight_9_num[My_position]; j++)
							{
								if (cstraight_9[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_9[My_position][j], 9);
								}
							}
						}
					}
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::STRAIGHT2)
			{
				if (packs.size() == 3)
				{
					if (cstraight2_3[My_position][cstraight2_3_num[My_position]] > comboLevel)
					{
						{
							for (int j = 1; j <= cstraight2_3_num[My_position]; j++)
							{
								if (cstraight2_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_3[My_position][j], 3);
								}
							}
						}
					}
				}
				else if (packs.size() == 4)
				{
					if (cstraight2_4[My_position][cstraight2_4_num[My_position]] > comboLevel)
					{
						{
							for (int j = 1; j <= cstraight2_4_num[My_position]; j++)
							{
								if (cstraight2_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_4[My_position][j], 4);
								}
							}
						}
					}
				}
				else if (packs.size() == 5)
				{
					if (cstraight2_5[My_position][cstraight2_5_num[My_position]] > comboLevel)
					{
						{
							for (int j = 1; j <= cstraight2_5_num[My_position]; j++)
							{
								if (cstraight2_5[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_5[My_position][j], 5);
								}
							}
						}
					}
				}
				else if (packs.size() == 6)
				{
					if (cstraight2_6[My_position][cstraight2_6_num[My_position]] > comboLevel)
					{
						{
							for (int j = 1; j <= cstraight2_6_num[My_position]; j++)
							{
								if (cstraight2_6[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_6[My_position][j], 6);
								}
							}
						}
					}
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::TRIPLET1)
			{
				if (csingle_num[My_position] > 0 && ctriplet_num[My_position] > 0)
				{
					if (ctriplet[My_position][ctriplet_num[My_position]] > comboLevel)
					{
						{
							for (int j = 1; j <= ctriplet_num[My_position]; j++)
							{
								if (ctriplet[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::TRIPLET1,
										ctriplet[My_position][j], 2, csingle[My_position]);
								}
							}
						}
					}
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::TRIPLET2)
			{
				if (cpair_num[My_position] > 0 && ctriplet_num[My_position] > 0)
				{
					if (ctriplet[My_position][ctriplet_num[My_position]] > comboLevel)
					{
						{
							for (int j = 1; j <= ctriplet_num[My_position]; j++)
							{
								if (ctriplet[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::TRIPLET2,
										ctriplet[My_position][j], 2, cpair[My_position]);
								}
							}
						}
					}
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::PLANE)
			{
				if (packs.size() == 2)
				{
					if (cstraight3_2[My_position][cstraight3_2_num[My_position]] > comboLevel)
					{
						{
							for (int j = 1; j <= cstraight3_2_num[My_position]; j++)
							{
								if (cstraight3_2[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE,
										cstraight3_2[My_position][j], 2);
								}
							}
						}
					}
				}
				else if (packs.size() == 3)
				{
					if (cstraight3_3[My_position][cstraight3_3_num[My_position]] > comboLevel)
					{
						{
							for (int j = 1; j <= cstraight3_3_num[My_position]; j++)
							{
								if (cstraight3_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE,
										cstraight3_3[My_position][j], 3);
								}
							}
						}
					}
				}
				else if (packs.size() == 4)
				{
					if (cstraight3_4[My_position][cstraight3_4_num[My_position]] > comboLevel)
					{
						{
							for (int j = 1; j <= cstraight3_4_num[My_position]; j++)
							{
								if (cstraight3_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE,
										cstraight3_4[My_position][j], 4);
								}
							}
						}
					}
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::PLANE1)
			{
				if (packs.size() / 2 == 2)
				{
					if (cstraight3_2[My_position][cstraight3_2_num[My_position]] > comboLevel && csingle_num[My_position] > packs.size() / 2)
					{
						{
							for (int j = 1; j <= cstraight3_2_num[My_position]; j++)
							{
								if (cstraight3_2[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE1,
										cstraight3_2[My_position][j], 2, csingle[My_position]);
								}
							}
						}
					}
				}
				else if (packs.size() / 2 == 3)
				{
					if (cstraight3_3[My_position][cstraight3_3_num[My_position]] > comboLevel && csingle_num[My_position] > packs.size() / 2)
					{
						{
							for (int j = 1; j <= cstraight3_3_num[My_position]; j++)
							{
								if (cstraight3_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE1,
										cstraight3_3[My_position][j], 3, csingle[My_position]);
								}
							}
						}
					}
				}
				else if (packs.size() / 2 == 4)
				{
					if (cstraight3_4[My_position][cstraight3_4_num[My_position]] > comboLevel && csingle_num[My_position] > packs.size() / 2)
					{
						{
							for (int j = 1; j <= cstraight3_4_num[My_position]; j++)
							{
								if (cstraight3_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE1,
										cstraight3_4[My_position][j], 4, csingle[My_position]);
								}
							}
						}
					}
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::PLANE2)
			{
				if (packs.size() / 2 == 2)
				{
					if (cstraight3_2[My_position][cstraight3_2_num[My_position]] > comboLevel && cpair_num[My_position] > packs.size() / 2)
					{
						{
							for (int j = 1; j <= cstraight3_2_num[My_position]; j++)
							{
								if (cstraight3_2[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE2,
										cstraight3_2[My_position][j], 2, cpair[My_position]);
								}
							}
						}
					}
				}
				else if (packs.size() / 2 == 3)
				{
					if (cstraight3_3[My_position][cstraight3_3_num[My_position]] > comboLevel && cpair_num[My_position] > packs.size() / 2)
					{
						{
							for (int j = 1; j <= cstraight3_3_num[My_position]; j++)
							{
								if (cstraight3_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE2,
										cstraight3_3[My_position][j], 3, cpair[My_position]);
								}
							}
						}
					}
				}
				else if (packs.size() / 2 == 4)
				{
					if (cstraight3_4[My_position][cstraight3_4_num[My_position]] > comboLevel && cpair_num[My_position] > packs.size() / 2)
					{
						{
							for (int j = 1; j <= cstraight3_4_num[My_position]; j++)
							{
								if (cstraight3_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE2,
										cstraight3_4[My_position][j], 4);
								}
							}
						}
					}
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::QUADRUPLE2)
			{
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::QUADRUPLE4)
			{
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::ROCKET)
			{
				return CardCombo(CardComboType::PASS);
			}
		}
	}
	else if (My_position == 2)//农民乙
	{
		if (this_combo_from == 0)
		{
			if (comboType == CardComboType::PASS)
			{
				if (cstraight2_6_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT2,
						cstraight2_6[My_position][cstraight2_6_num[My_position]], 6);
				}
				if (cstraight2_5_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT2,
						cstraight2_5[My_position][cstraight2_5_num[My_position]], 5);
				}
				if (cstraight2_4_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT2,
						cstraight2_4[My_position][cstraight2_4_num[My_position]], 4);
				}
				if (cstraight2_3_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT2,
						cstraight2_3[My_position][cstraight2_3_num[My_position]], 3);
				}
				if (cstraight_9_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT,
						cstraight_9[My_position][cstraight_9_num[My_position]], 9);
				}
				if (cstraight_8_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT,
						cstraight_8[My_position][cstraight_8_num[My_position]], 8);
				}
				if (cstraight_7_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT,
						cstraight_7[My_position][cstraight_7_num[My_position]], 7);
				}
				if (cstraight_6_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT,
						cstraight_6[My_position][cstraight_6_num[My_position]], 6);
				}
				if (cstraight_5_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT,
						cstraight_5[My_position][cstraight_5_num[My_position]], 5);
				}
				if (cstraight3_4_num[My_position] > 0)
				{
					if (csingle_num[My_position] >= 4)
					{
						return CardCombo(CardComboType::PLANE1,
							cstraight3_4[My_position][cstraight3_4_num[My_position]], 4, csingle[My_position]);
					}
					if (cpair_num[My_position] >= 4)
					{
						return CardCombo(CardComboType::PLANE2,
							cstraight3_4[My_position][cstraight3_4_num[My_position]], 4, cpair[My_position]);
					}
				}
				if (cstraight3_3_num[My_position] > 0)
				{
					if (csingle_num[My_position] >= 3)
					{
						return CardCombo(CardComboType::PLANE1,
							cstraight3_3[My_position][cstraight3_3_num[My_position]], 3, csingle[My_position]);
					}
					else if (cpair_num[My_position] >= 3)
					{
						return CardCombo(CardComboType::PLANE2,
							cstraight3_3[My_position][cstraight3_3_num[My_position]], 3, cpair[My_position]);
					}
				}
				if (cstraight3_2_num[My_position] > 0)
				{
					if (csingle_num[My_position] >= 2)
					{
						return CardCombo(CardComboType::PLANE1,
							cstraight3_2[My_position][cstraight3_2_num[My_position]], 2, csingle[My_position]);
					}
					if (cpair_num[My_position] >= 2)
					{
						return CardCombo(CardComboType::PLANE2,
							cstraight3_2[My_position][cstraight3_2_num[My_position]], 2, cpair[My_position]);
					}
				}
				if (ctriplet_num[My_position] > 0)
				{
					if (csingle_num[My_position] > 0)
					{
						return CardCombo(CardComboType::TRIPLET1,
							ctriplet[My_position][1], 1, csingle[My_position]);
					}
					if (cpair_num[My_position] > 0)
					{
						return CardCombo(CardComboType::TRIPLET2,
							ctriplet[My_position][1], 1, cpair[My_position]);
					}
					return CardCombo(CardComboType::TRIPLET,
						ctriplet[My_position][1], 1);
				}
				if (cstraight3_4_num[My_position] > 0)
				{
					return CardCombo(CardComboType::PLANE,
						cstraight3_4[My_position][cstraight3_4_num[My_position]], 4);
				}
				if (cstraight3_3_num[My_position] > 0)
				{
					return CardCombo(CardComboType::PLANE,
						cstraight3_3[My_position][cstraight3_3_num[My_position]], 3);
				}
				if (cstraight3_2_num[My_position] > 0)
				{
					return CardCombo(CardComboType::PLANE,
						cstraight3_2[My_position][cstraight3_2_num[My_position]], 2);
				}
				if (cpair_num[My_position] != 0)
				{
					return CardCombo(CardComboType::PAIR,
						cpair[My_position][1], 1);
				}
				if (csingle_num[My_position] != 0)
				{
					return CardCombo(CardComboType::SINGLE,
						csingle[My_position][1], 1);
				}
				if (cbomb_num[My_position] != 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				if (rocket[My_position] != 0)
				{
					return CardCombo(CardComboType::ROCKET,
						15, 1);
				}
			}
			else if (comboType == CardComboType::SINGLE)
			{
				if (csingle[My_position][csingle_num[My_position]] > comboLevel)
				{
					if (csingle[My_position][csingle_num[My_position]] >= csingle[0][csingle_num[0]])
					{
						for (int j = 1; j <= csingle_num[My_position]; j++)
						{
							if (csingle[My_position][j] >= csingle[0][csingle_num[0]] && csingle[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::SINGLE,
									csingle[My_position][j], 1);
							}
						}
					}
					else
					{
						for (int j = 1; j <= csingle_num[My_position]; j++)
						{
							if (csingle[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::SINGLE,
									csingle[My_position][j], 1);
							}
						}
					}
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::PAIR)
			{
				if (cpair[My_position][cpair_num[My_position]] > comboLevel)
				{

					if (cpair[My_position][cpair_num[My_position]] >= cpair[0][cpair_num[0]])
					{
						for (int j = 1; j <= cpair_num[My_position]; j++)
						{
							if (cpair[My_position][j] >= cpair[0][cpair_num[0]]
								&& cpair[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::PAIR,
									cpair[My_position][j], 1);
							}
						}
					}

					else
					{
						for (int j = 1; j <= cpair_num[My_position]; j++)
						{
							if (cpair[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::PAIR,
									cpair[My_position][j], 1);
							}
						}
					}
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::TRIPLET)
			{
				if (ctriplet[My_position][ctriplet_num[My_position]] > comboLevel)
				{

					if (ctriplet[My_position][ctriplet_num[My_position]] >= ctriplet[0][ctriplet_num[0]])
					{
						for (int j = 1; j <= ctriplet_num[My_position]; j++)
						{
							if (ctriplet[My_position][j] >= ctriplet[0][ctriplet_num[0]] &&
								ctriplet[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::TRIPLET,
									ctriplet[My_position][j], 1);
							}
						}
					}

					else
					{
						for (int j = 1; j <= ctriplet_num[My_position]; j++)
						{
							if (ctriplet[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::TRIPLET,
									ctriplet[My_position][j], 1);
							}
						}
					}
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::BOMB)
			{
				if (cbomb[My_position][cbomb_num[My_position]] > comboLevel)
				{

					if (cbomb[My_position][cbomb_num[My_position]] >= cbomb[0][cbomb_num[0]])
					{
						for (int j = 1; j <= cbomb_num[My_position]; j++)
						{
							if (cbomb[My_position][j] >= cbomb[0][cbomb_num[0]]
								&& cbomb[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::BOMB,
									cbomb[My_position][j], 1);
							}
						}

					}
					else
					{
						for (int j = 1; j <= cbomb_num[My_position]; j++)
						{
							if (cbomb[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::BOMB,
									cbomb[My_position][j], 1);
							}
						}
					}
				}
				if (rocket[My_position] == 1)
				{
					return CardCombo(CardComboType::ROCKET);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::STRAIGHT)
			{
				if (packs.size() == 5)
				{
					if (cstraight_5[My_position][cstraight_5_num[My_position]] > comboLevel)
					{

						if (cstraight_5[My_position][cstraight_5_num[My_position]] >= cstraight_5[0][cstraight_5_num[0]])
						{
							for (int j = 1; j <= cstraight_5_num[My_position]; j++)
							{
								if (cstraight_5[My_position][j] >= cstraight_5[0][cstraight_5_num[0]]
									&& cstraight_5[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_5[My_position][j], 5);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight_5_num[My_position]; j++)
							{
								if (cstraight_5[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_5[My_position][j], 5);
								}
							}
						}
					}
				}
				else if (packs.size() == 6)
				{
					if (cstraight_6[My_position][cstraight_6_num[My_position]] > comboLevel)
					{

						if (cstraight_6[My_position][cstraight_6_num[My_position]] >= cstraight_6[0][cstraight_6_num[0]])
						{
							for (int j = 1; j <= cstraight_6_num[My_position]; j++)
							{
								if (cstraight_6[My_position][j] >= cstraight_6[0][cstraight_6_num[0]] && cstraight_6[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_6[My_position][j], 6);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight_6_num[My_position]; j++)
							{
								if (cstraight_6[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_6[My_position][j], 6);
								}
							}
						}
					}
				}
				else if (packs.size() == 7)
				{
					if (cstraight_7[My_position][cstraight_7_num[My_position]] > comboLevel)
					{

						if (cstraight_7[My_position][cstraight_7_num[My_position]] >= cstraight_7[0][cstraight_7_num[0]])
						{
							for (int j = 1; j <= cstraight_7_num[My_position]; j++)
							{
								if (cstraight_7[My_position][j] >= cstraight_7[0][cstraight_7_num[0]] && cstraight_7[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_7[My_position][j], 7);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight_7_num[My_position]; j++)
							{
								if (cstraight_7[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_7[My_position][j], 7);
								}
							}
						}
					}
				}
				else if (packs.size() == 8)
				{
					if (cstraight_8[My_position][cstraight_8_num[My_position]] > comboLevel)
					{

						if (cstraight_8[My_position][cstraight_8_num[My_position]] >= cstraight_8[0][cstraight_8_num[0]])
						{
							for (int j = 1; j <= cstraight_8_num[My_position]; j++)
							{
								if (cstraight_8[My_position][j] >= cstraight_8[0][cstraight_8_num[0]]
									&& cstraight_8[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_8[My_position][j], 8);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight_8_num[My_position]; j++)
							{
								if (cstraight_8[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_8[My_position][j], 8);
								}
							}
						}
					}
				}
				else if (packs.size() == 9)
				{
					if (cstraight_9[My_position][cstraight_9_num[My_position]] > comboLevel)
					{

						if (cstraight_9[My_position][cstraight_9_num[My_position]] >= cstraight_9[0][cstraight_9_num[0]])
						{
							for (int j = 1; j <= cstraight_9_num[My_position]; j++)
							{
								if (cstraight_9[My_position][j] >= cstraight_9[0][cstraight_9_num[0]]
									&& cstraight_9[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_9[My_position][j], 9);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight_9_num[My_position]; j++)
							{
								if (cstraight_9[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_9[My_position][j], 9);
								}
							}
						}
					}
				}
				if (cbomb_num[My_position] > 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				if (rocket[My_position] == 1)
				{
					return CardCombo(CardComboType::ROCKET);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::STRAIGHT2)
			{
				if (packs.size() == 3)
				{
					if (cstraight2_3[My_position][cstraight2_3_num[My_position]] > comboLevel)
					{

						if (cstraight2_3[My_position][cstraight2_3_num[My_position]] >= cstraight2_3[0][cstraight2_3_num[0]])
						{
							for (int j = 1; j <= cstraight2_3_num[My_position]; j++)
							{
								if (cstraight2_3[My_position][j] >= cstraight2_3[0][cstraight2_3_num[0]]
									&& cstraight2_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_3[My_position][j], 3);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight2_3_num[My_position]; j++)
							{
								if (cstraight2_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_3[My_position][j], 3);
								}
							}
						}
					}
				}
				else if (packs.size() == 4)
				{
					if (cstraight2_4[My_position][cstraight2_4_num[My_position]] > comboLevel)
					{

						if (cstraight2_4[My_position][cstraight2_4_num[My_position]] >= cstraight2_4[0][cstraight2_4_num[0]])
						{
							for (int j = 1; j <= cstraight2_4_num[My_position]; j++)
							{
								if (cstraight2_4[My_position][j] >= cstraight2_4[0][cstraight2_4_num[0]] && cstraight2_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_4[My_position][j], 4);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight2_4_num[My_position]; j++)
							{
								if (cstraight2_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_4[My_position][j], 4);
								}
							}
						}
					}
				}
				else if (packs.size() == 5)
				{
					if (cstraight2_5[My_position][cstraight2_5_num[My_position]] > comboLevel)
					{

						if (cstraight2_5[My_position][cstraight2_5_num[My_position]] >= cstraight2_5[0][cstraight2_5_num[0]])
						{
							for (int j = 1; j <= cstraight2_5_num[My_position]; j++)
							{
								if (cstraight2_5[My_position][j] >= cstraight2_5[0][cstraight2_5_num[0]] && cstraight2_5[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_5[My_position][j], 5);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight2_5_num[My_position]; j++)
							{
								if (cstraight2_5[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_5[My_position][j], 5);
								}
							}
						}
					}
				}
				else if (packs.size() == 6)
				{
					if (cstraight2_6[My_position][cstraight2_6_num[My_position]] > comboLevel)
					{

						if (cstraight2_6[My_position][cstraight2_6_num[My_position]] >= cstraight2_6[0][cstraight2_6_num[0]])
						{
							for (int j = 1; j <= cstraight2_6_num[My_position]; j++)
							{
								if (cstraight2_6[My_position][j] >= cstraight2_6[0][cstraight2_6_num[0]] && cstraight2_6[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_6[My_position][j], 6);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight2_6_num[My_position]; j++)
							{
								if (cstraight2_6[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_6[My_position][j], 6);
								}
							}
						}
					}
				}
				if (cbomb_num[My_position] > 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				if (rocket[My_position] == 1)
				{
					return CardCombo(CardComboType::ROCKET);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::TRIPLET1)
			{
				if (csingle_num[My_position] > 0 && ctriplet_num[My_position] > 0)
				{
					if (ctriplet[My_position][ctriplet_num[My_position]] > comboLevel)
					{
						if (ctriplet[My_position][ctriplet_num[My_position]] >= ctriplet[0][ctriplet_num[0]])
						{
							for (int j = 1; j <= ctriplet_num[My_position]; j++)
							{
								if (ctriplet[My_position][j] >= ctriplet[0][ctriplet_num[0]] && ctriplet[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::TRIPLET1,
										ctriplet[My_position][j], 2, csingle[My_position]);
								}
							}
						}
						else
						{
							for (int j = 1; j <= ctriplet_num[My_position]; j++)
							{
								if (ctriplet[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::TRIPLET1,
										ctriplet[My_position][j], 2, csingle[My_position]);
								}
							}
						}
					}
				}
				if (cbomb_num[My_position] > 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				if (rocket[My_position] == 1)
				{
					return CardCombo(CardComboType::ROCKET);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::TRIPLET2)
			{
				if (cpair_num[My_position] > 0 && ctriplet_num[My_position] > 0)
				{
					if (ctriplet[My_position][ctriplet_num[My_position]] > comboLevel)
					{
						if (ctriplet[My_position][ctriplet_num[My_position]] >= ctriplet[0][ctriplet_num[0]])
						{
							for (int j = 1; j <= ctriplet_num[My_position]; j++)
							{
								if (ctriplet[My_position][j] >= ctriplet[0][ctriplet_num[0]] && ctriplet[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::TRIPLET2,
										ctriplet[My_position][j], 2, cpair[My_position]);
								}
							}
						}
						else
						{
							for (int j = 1; j <= ctriplet_num[My_position]; j++)
							{
								if (ctriplet[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::TRIPLET2,
										ctriplet[My_position][j], 2, cpair[My_position]);
								}
							}
						}
					}
				}
				if (cbomb_num[My_position] > 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				if (rocket[My_position] == 1)
				{
					return CardCombo(CardComboType::ROCKET);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::PLANE)
			{
				if (packs.size() == 2)
				{
					if (cstraight3_2[My_position][cstraight3_2_num[My_position]] > comboLevel)
					{

						if (cstraight3_2[My_position][cstraight3_2_num[My_position]] >= cstraight3_2[0][cstraight3_2_num[0]])
						{
							for (int j = 1; j <= cstraight3_2_num[My_position]; j++)
							{
								if (cstraight3_2[My_position][j] >= cstraight3_2[0][cstraight3_2_num[0]] && cstraight3_2[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE,
										cstraight3_2[My_position][j], 2);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight3_2_num[My_position]; j++)
							{
								if (cstraight3_2[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE,
										cstraight3_2[My_position][j], 2);
								}
							}
						}
					}
				}
				else if (packs.size() == 3)
				{
					if (cstraight3_3[My_position][cstraight3_3_num[My_position]] > comboLevel)
					{

						if (cstraight3_3[My_position][cstraight3_3_num[My_position]] >= cstraight3_3[0][cstraight3_3_num[0]])
						{
							for (int j = 1; j <= cstraight3_3_num[My_position]; j++)
							{
								if (cstraight3_3[My_position][j] >= cstraight3_3[0][cstraight3_3_num[0]] && cstraight3_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE,
										cstraight3_3[My_position][j], 3);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight3_3_num[My_position]; j++)
							{
								if (cstraight3_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE,
										cstraight3_3[My_position][j], 3);
								}
							}
						}
					}
				}
				else if (packs.size() == 4)
				{
					if (cstraight3_4[My_position][cstraight3_4_num[My_position]] > comboLevel)
					{

						if (cstraight3_4[My_position][cstraight3_4_num[My_position]] >= cstraight3_4[0][cstraight3_4_num[0]])
						{
							for (int j = 1; j <= cstraight3_4_num[My_position]; j++)
							{
								if (cstraight3_4[My_position][j] >= cstraight3_4[0][cstraight3_4_num[0]] && cstraight3_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE,
										cstraight3_4[My_position][j], 4);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight3_4_num[My_position]; j++)
							{
								if (cstraight3_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE,
										cstraight3_4[My_position][j], 4);
								}
							}
						}
					}
				}
				if (cbomb_num[My_position] > 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				if (rocket[My_position] == 1)
				{
					return CardCombo(CardComboType::ROCKET);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::PLANE1)
			{
				if (packs.size() / 2 == 2)
				{
					if (cstraight3_2[My_position][cstraight3_2_num[My_position]] > comboLevel && csingle_num[My_position] > packs.size() / 2)
					{

						if (cstraight3_2[My_position][cstraight3_2_num[My_position]] >= cstraight3_2[0][cstraight3_2_num[0]])
						{
							for (int j = 1; j <= cstraight3_2_num[My_position]; j++)
							{
								if (cstraight3_2[My_position][j] >= cstraight3_2[0][cstraight3_2_num[0]] && cstraight3_2[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE1,
										cstraight3_2[My_position][j], 2, csingle[My_position]);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight3_2_num[My_position]; j++)
							{
								if (cstraight3_2[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE1,
										cstraight3_2[My_position][j], 2, csingle[My_position]);
								}
							}
						}
					}
				}
				else if (packs.size() / 2 == 3)
				{
					if (cstraight3_3[My_position][cstraight3_3_num[My_position]] > comboLevel && csingle_num[My_position] > packs.size() / 2)
					{

						if (cstraight3_3[My_position][cstraight3_3_num[My_position]] >= cstraight3_3[0][cstraight3_3_num[0]])
						{
							for (int j = 1; j <= cstraight3_3_num[My_position]; j++)
							{
								if (cstraight3_3[My_position][j] >= cstraight3_3[0][cstraight3_3_num[0]] && cstraight3_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE1,
										cstraight3_3[My_position][j], 3, csingle[My_position]);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight3_3_num[My_position]; j++)
							{
								if (cstraight3_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE1,
										cstraight3_3[My_position][j], 3, csingle[My_position]);
								}
							}
						}
					}
				}
				else if (packs.size() / 2 == 4)
				{
					if (cstraight3_4[My_position][cstraight3_4_num[My_position]] > comboLevel && csingle_num[My_position] > packs.size() / 2)
					{

						if (cstraight3_4[My_position][cstraight3_4_num[My_position]] >= cstraight3_4[0][cstraight3_4_num[0]])
						{
							for (int j = 1; j <= cstraight3_4_num[My_position]; j++)
							{
								if (cstraight3_4[My_position][j] >= cstraight3_4[0][cstraight3_4_num[0]] && cstraight3_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE1,
										cstraight3_4[My_position][j], 4, csingle[My_position]);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight3_4_num[My_position]; j++)
							{
								if (cstraight3_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE1,
										cstraight3_4[My_position][j], 4, csingle[My_position]);
								}
							}
						}
					}
				}
				if (cbomb_num[My_position] > 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				if (rocket[My_position] == 1)
				{
					return CardCombo(CardComboType::ROCKET);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::PLANE2)
			{
				if (packs.size() / 2 == 2)
				{
					if (cstraight3_2[My_position][cstraight3_2_num[My_position]] > comboLevel && cpair_num[My_position] > packs.size() / 2)
					{

						if (cstraight3_2[My_position][cstraight3_2_num[My_position]] >= cstraight3_2[0][cstraight3_2_num[0]])
						{
							for (int j = 1; j <= cstraight3_2_num[My_position]; j++)
							{
								if (cstraight3_2[My_position][j] >= cstraight3_2[0][cstraight3_2_num[0]] && cstraight3_2[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE2,
										cstraight3_2[My_position][j], 2, cpair[My_position]);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight3_2_num[My_position]; j++)
							{
								if (cstraight3_2[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE2,
										cstraight3_2[My_position][j], 2, cpair[My_position]);
								}
							}
						}
					}
				}
				else if (packs.size() / 2 == 3)
				{
					if (cstraight3_3[My_position][cstraight3_3_num[My_position]] > comboLevel && cpair_num[My_position] > packs.size() / 2)
					{

						if (cstraight3_3[My_position][cstraight3_3_num[My_position]] >= cstraight3_3[0][cstraight3_3_num[0]])
						{
							for (int j = 1; j <= cstraight3_3_num[My_position]; j++)
							{
								if (cstraight3_3[My_position][j] >= cstraight3_3[0][cstraight3_3_num[0]] && cstraight3_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE2,
										cstraight3_3[My_position][j], 3, cpair[My_position]);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight3_3_num[My_position]; j++)
							{
								if (cstraight3_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE2,
										cstraight3_3[My_position][j], 3, cpair[My_position]);
								}
							}
						}
					}
				}
				else if (packs.size() / 2 == 4)
				{
					if (cstraight3_4[My_position][cstraight3_4_num[My_position]] > comboLevel && cpair_num[My_position] > packs.size() / 2)
					{

						if (cstraight3_4[My_position][cstraight3_4_num[My_position]] >= cstraight3_4[0][cstraight3_4_num[0]])
						{
							for (int j = 1; j <= cstraight3_4_num[My_position]; j++)
							{
								if (cstraight3_4[My_position][j] >= cstraight3_4[0][cstraight3_4_num[0]] && cstraight3_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE2,
										cstraight3_4[My_position][j], 4);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight3_4_num[My_position]; j++)
							{
								if (cstraight3_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE2,
										cstraight3_4[My_position][j], 4);
								}
							}
						}
					}
				}
				if (cbomb_num[My_position] > 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				if (rocket[My_position] == 1)
				{
					return CardCombo(CardComboType::ROCKET);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::QUADRUPLE2)
			{
				if (cbomb_num[My_position] > 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::QUADRUPLE4)
			{
				if (cbomb_num[My_position] > 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::ROCKET)
			{
				return CardCombo(CardComboType::PASS);
			}
		}
		if (this_combo_from == 1)
		{
			if (comboType == CardComboType::PASS)
			{
				if (cstraight2_6_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT2,
						cstraight2_6[My_position][cstraight2_6_num[My_position]], 6);
				}
				if (cstraight2_5_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT2,
						cstraight2_5[My_position][cstraight2_5_num[My_position]], 5);
				}
				if (cstraight2_4_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT2,
						cstraight2_4[My_position][cstraight2_4_num[My_position]], 4);
				}
				if (cstraight2_3_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT2,
						cstraight2_3[My_position][cstraight2_3_num[My_position]], 3);
				}
				if (cstraight_9_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT,
						cstraight_9[My_position][cstraight_9_num[My_position]], 9);
				}
				if (cstraight_8_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT,
						cstraight_8[My_position][cstraight_8_num[My_position]], 8);
				}
				if (cstraight_7_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT,
						cstraight_7[My_position][cstraight_7_num[My_position]], 7);
				}
				if (cstraight_6_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT,
						cstraight_6[My_position][cstraight_6_num[My_position]], 6);
				}
				if (cstraight_5_num[My_position] > 0)
				{
					return CardCombo(CardComboType::STRAIGHT,
						cstraight_5[My_position][cstraight_5_num[My_position]], 5);
				}
				if (cstraight3_4_num[My_position] > 0)
				{
					if (csingle_num[My_position] >= 4)
					{
						return CardCombo(CardComboType::PLANE1,
							cstraight3_4[My_position][cstraight3_4_num[My_position]], 4, csingle[My_position]);
					}
					if (cpair_num[My_position] >= 4)
					{
						return CardCombo(CardComboType::PLANE2,
							cstraight3_4[My_position][cstraight3_4_num[My_position]], 4, cpair[My_position]);
					}
				}
				if (cstraight3_3_num[My_position] > 0)
				{
					if (csingle_num[My_position] >= 3)
					{
						return CardCombo(CardComboType::PLANE1,
							cstraight3_3[My_position][cstraight3_3_num[My_position]], 3, csingle[My_position]);
					}
					else if (cpair_num[My_position] >= 3)
					{
						return CardCombo(CardComboType::PLANE2,
							cstraight3_3[My_position][cstraight3_3_num[My_position]], 3, cpair[My_position]);
					}
				}
				if (cstraight3_2_num[My_position] > 0)
				{
					if (csingle_num[My_position] >= 2)
					{
						return CardCombo(CardComboType::PLANE1,
							cstraight3_2[My_position][cstraight3_2_num[My_position]], 2, csingle[My_position]);
					}
					if (cpair_num[My_position] >= 2)
					{
						return CardCombo(CardComboType::PLANE2,
							cstraight3_2[My_position][cstraight3_2_num[My_position]], 2, cpair[My_position]);
					}
				}
				if (ctriplet_num[My_position] > 0)
				{
					if (csingle_num[My_position] > 0)
					{
						return CardCombo(CardComboType::TRIPLET1,
							ctriplet[My_position][1], 2, csingle[My_position]);
					}
					if (cpair_num[My_position] > 0)
					{
						return CardCombo(CardComboType::TRIPLET1,
							ctriplet[My_position][1], 2, cpair[My_position]);
					}
					return CardCombo(CardComboType::TRIPLET,
						ctriplet[My_position][1], 1);
				}
				if (cstraight3_4_num[My_position] > 0)
				{
					return CardCombo(CardComboType::PLANE,
						cstraight3_4[My_position][cstraight3_4_num[My_position]], 4);
				}
				if (cstraight3_3_num[My_position] > 0)
				{
					return CardCombo(CardComboType::PLANE,
						cstraight3_3[My_position][cstraight3_3_num[My_position]], 3);
				}
				if (cstraight3_2_num[My_position] > 0)
				{
					return CardCombo(CardComboType::PLANE,
						cstraight3_2[My_position][cstraight3_2_num[My_position]], 2);
				}
				if (cpair_num[My_position] != 0)
				{
					return CardCombo(CardComboType::PAIR,
						cpair[My_position][1], 1);
				}
				if (csingle_num[My_position] != 0)
				{
					return CardCombo(CardComboType::SINGLE,
						csingle[My_position][1], 1);
				}
				if (cbomb_num[My_position] != 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				if (rocket[My_position] != 0)
				{
					return CardCombo(CardComboType::ROCKET,
						15, 1);
				}
			}
			else if (comboType == CardComboType::SINGLE)
			{
				if (csingle[My_position][csingle_num[My_position]] > comboLevel)
				{
					if (csingle[My_position][csingle_num[My_position]] >= csingle[0][csingle_num[0]] && csingle[0][csingle_num[0]] > comboLevel)
					{
						for (int j = 1; j <= csingle_num[My_position]; j++)
						{
							if (csingle[My_position][j] >= csingle[0][csingle_num[0]] && csingle[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::SINGLE,
									csingle[My_position][j], 1);
							}
						}
					}
					else
					{
						for (int j = 1; j <= csingle_num[My_position]; j++)
						{
							if (csingle[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::SINGLE,
									csingle[My_position][j], 1);
							}
						}
					}
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::PAIR)
			{
				if (cpair[My_position][cpair_num[My_position]] > comboLevel)
				{

					if (cpair[My_position][cpair_num[My_position]] >= cpair[0][cpair_num[0]] && cpair[0][cpair_num[0]] > comboLevel)
					{
						for (int j = 1; j <= cpair_num[My_position]; j++)
						{
							if (cpair[My_position][j] >= cpair[0][cpair_num[0]]
								&& cpair[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::PAIR,
									cpair[My_position][j], 1);
							}
						}
					}

					else
					{
						for (int j = 1; j <= cpair_num[My_position]; j++)
						{
							if (cpair[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::PAIR,
									cpair[My_position][j], 1);
							}
						}
					}
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::TRIPLET)
			{
				if (ctriplet[My_position][ctriplet_num[My_position]] > comboLevel)
				{

					if (ctriplet[My_position][ctriplet_num[My_position]] >= ctriplet[0][ctriplet_num[0]] && ctriplet[0][ctriplet_num[0]] > comboLevel)
					{
						for (int j = 1; j <= ctriplet_num[My_position]; j++)
						{
							if (ctriplet[My_position][j] >= ctriplet[0][ctriplet_num[0]] &&
								ctriplet[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::TRIPLET,
									ctriplet[My_position][j], 1);
							}
						}
					}

					else
					{
						for (int j = 1; j <= ctriplet_num[My_position]; j++)
						{
							if (ctriplet[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::TRIPLET,
									ctriplet[My_position][j], 1);
							}
						}
					}
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::BOMB)
			{
				if (cbomb[My_position][cbomb_num[My_position]] > comboLevel)
				{

					if (cbomb[My_position][cbomb_num[My_position]] >= cbomb[0][cbomb_num[0]] && cbomb[0][cbomb_num[0]] > comboLevel)
					{
						for (int j = 1; j <= cbomb_num[My_position]; j++)
						{
							if (cbomb[My_position][j] >= cbomb[0][cbomb_num[0]]
								&& cbomb[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::BOMB,
									cbomb[My_position][j], 1);
							}
						}

					}
					else
					{
						for (int j = 1; j <= cbomb_num[My_position]; j++)
						{
							if (cbomb[My_position][j] > comboLevel)
							{
								return CardCombo(CardComboType::BOMB,
									cbomb[My_position][j], 1);
							}
						}
					}
				}
				if (rocket[My_position] == 1)
				{
					return CardCombo(CardComboType::ROCKET);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::STRAIGHT)
			{
				if (packs.size() == 5)
				{
					if (cstraight_5[My_position][cstraight_5_num[My_position]] > comboLevel)
					{

						if (cstraight_5[My_position][cstraight_5_num[My_position]] >= cstraight_5[0][cstraight_5_num[0]] && cstraight_5[0][cstraight_5_num[0]] > comboLevel)
						{
							for (int j = 1; j <= cstraight_5_num[My_position]; j++)
							{
								if (cstraight_5[My_position][j] >= cstraight_5[0][cstraight_5_num[0]]
									&& cstraight_5[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_5[My_position][j], 5);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight_5_num[My_position]; j++)
							{
								if (cstraight_5[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_5[My_position][j], 5);
								}
							}
						}
					}
				}
				else if (packs.size() == 6)
				{
					if (cstraight_6[My_position][cstraight_6_num[My_position]] > comboLevel)
					{

						if (cstraight_6[My_position][cstraight_6_num[My_position]] >= cstraight_6[0][cstraight_6_num[0]] && cstraight_6[0][cstraight_6_num[0]] > comboLevel)
						{
							for (int j = 1; j <= cstraight_6_num[My_position]; j++)
							{
								if (cstraight_6[My_position][j] >= cstraight_6[0][cstraight_6_num[0]] && cstraight_6[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_6[My_position][j], 6);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight_6_num[My_position]; j++)
							{
								if (cstraight_6[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_6[My_position][j], 6);
								}
							}
						}
					}
				}
				else if (packs.size() == 7)
				{
					if (cstraight_7[My_position][cstraight_7_num[My_position]] > comboLevel)
					{

						if (cstraight_7[My_position][cstraight_7_num[My_position]] >= cstraight_7[0][cstraight_7_num[0]] && cstraight_7[0][cstraight_7_num[0]] > comboLevel)
						{
							for (int j = 1; j <= cstraight_7_num[My_position]; j++)
							{
								if (cstraight_7[My_position][j] >= cstraight_7[0][cstraight_7_num[0]] && cstraight_7[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_7[My_position][j], 7);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight_7_num[My_position]; j++)
							{
								if (cstraight_7[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_7[My_position][j], 7);
								}
							}
						}
					}
				}
				else if (packs.size() == 8)
				{
					if (cstraight_8[My_position][cstraight_8_num[My_position]] > comboLevel)
					{

						if (cstraight_8[My_position][cstraight_8_num[My_position]] >= cstraight_8[0][cstraight_8_num[0]] && cstraight_8[0][cstraight_8_num[0]] > comboLevel)
						{
							for (int j = 1; j <= cstraight_8_num[My_position]; j++)
							{
								if (cstraight_8[My_position][j] >= cstraight_8[0][cstraight_8_num[0]]
									&& cstraight_8[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_8[My_position][j], 8);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight_8_num[My_position]; j++)
							{
								if (cstraight_8[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_8[My_position][j], 8);
								}
							}
						}
					}
				}
				else if (packs.size() == 9)
				{
					if (cstraight_9[My_position][cstraight_9_num[My_position]] > comboLevel)
					{

						if (cstraight_9[My_position][cstraight_9_num[My_position]] >= cstraight_9[0][cstraight_9_num[0]] && cstraight_9[0][cstraight_9_num[0]] > comboLevel)
						{
							for (int j = 1; j <= cstraight_9_num[My_position]; j++)
							{
								if (cstraight_9[My_position][j] >= cstraight_9[0][cstraight_9_num[0]]
									&& cstraight_9[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_9[My_position][j], 9);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight_9_num[My_position]; j++)
							{
								if (cstraight_9[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT,
										cstraight_9[My_position][j], 9);
								}
							}
						}
					}
				}
				if (cbomb_num[My_position] > 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				if (rocket[My_position] == 1)
				{
					return CardCombo(CardComboType::ROCKET);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::STRAIGHT2)
			{
				if (packs.size() == 3)
				{
					if (cstraight2_3[My_position][cstraight2_3_num[My_position]] > comboLevel)
					{

						if (cstraight2_3[My_position][cstraight2_3_num[My_position]] >= cstraight2_3[0][cstraight2_3_num[0]] && cstraight2_3[0][cstraight2_3_num[0]] > comboLevel)
						{
							for (int j = 1; j <= cstraight2_3_num[My_position]; j++)
							{
								if (cstraight2_3[My_position][j] >= cstraight2_3[0][cstraight2_3_num[0]]
									&& cstraight2_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_3[My_position][j], 3);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight2_3_num[My_position]; j++)
							{
								if (cstraight2_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_3[My_position][j], 3);
								}
							}
						}
					}
				}
				else if (packs.size() == 4)
				{
					if (cstraight2_4[My_position][cstraight2_4_num[My_position]] > comboLevel)
					{

						if (cstraight2_4[My_position][cstraight2_4_num[My_position]] >= cstraight2_4[0][cstraight2_4_num[0]] && cstraight2_4[0][cstraight2_4_num[0]] > comboLevel)
						{
							for (int j = 1; j <= cstraight2_4_num[My_position]; j++)
							{
								if (cstraight2_4[My_position][j] >= cstraight2_4[0][cstraight2_4_num[0]] && cstraight2_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_4[My_position][j], 4);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight2_4_num[My_position]; j++)
							{
								if (cstraight2_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_4[My_position][j], 4);
								}
							}
						}
					}
				}
				else if (packs.size() == 5)
				{
					if (cstraight2_5[My_position][cstraight2_5_num[My_position]] > comboLevel)
					{

						if (cstraight2_5[My_position][cstraight2_5_num[My_position]] >= cstraight2_5[0][cstraight2_5_num[0]] && cstraight2_5[0][cstraight2_5_num[0]] > comboLevel)
						{
							for (int j = 1; j <= cstraight2_5_num[My_position]; j++)
							{
								if (cstraight2_5[My_position][j] >= cstraight2_5[0][cstraight2_5_num[0]] && cstraight2_5[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_5[My_position][j], 5);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight2_5_num[My_position]; j++)
							{
								if (cstraight2_5[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_5[My_position][j], 5);
								}
							}
						}
					}
				}
				else if (packs.size() == 6)
				{
					if (cstraight2_6[My_position][cstraight2_6_num[My_position]] > comboLevel)
					{

						if (cstraight2_6[My_position][cstraight2_6_num[My_position]] >= cstraight2_6[0][cstraight2_6_num[0]] && cstraight2_6[0][cstraight2_6_num[0]] > comboLevel)
						{
							for (int j = 1; j <= cstraight2_6_num[My_position]; j++)
							{
								if (cstraight2_6[My_position][j] >= cstraight2_6[0][cstraight2_6_num[0]] && cstraight2_6[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_6[My_position][j], 6);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight2_6_num[My_position]; j++)
							{
								if (cstraight2_6[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::STRAIGHT2,
										cstraight2_6[My_position][j], 6);
								}
							}
						}
					}
				}
				if (cbomb_num[My_position] > 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				if (rocket[My_position] == 1)
				{
					return CardCombo(CardComboType::ROCKET);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::TRIPLET1)
			{
				if (csingle_num[My_position] > 0 && ctriplet_num[My_position] > 0)
				{
					if (ctriplet[My_position][ctriplet_num[My_position]] > comboLevel)
					{
						if (ctriplet[My_position][ctriplet_num[My_position]] >= ctriplet[0][ctriplet_num[0]] && ctriplet[0][ctriplet_num[0]] > comboLevel)
						{
							for (int j = 1; j <= ctriplet_num[My_position]; j++)
							{
								if (ctriplet[My_position][j] >= ctriplet[0][ctriplet_num[0]] && ctriplet[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::TRIPLET1,
										ctriplet[My_position][j], 2, csingle[My_position]);
								}
							}
						}
						else
						{
							for (int j = 1; j <= ctriplet_num[My_position]; j++)
							{
								if (ctriplet[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::TRIPLET1,
										ctriplet[My_position][j], 2, csingle[My_position]);
								}
							}
						}
					}
				}
				if (cbomb_num[My_position] > 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				if (rocket[My_position] == 1)
				{
					return CardCombo(CardComboType::ROCKET);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::TRIPLET2)
			{
				if (cpair_num[My_position] > 0 && ctriplet_num[My_position] > 0)
				{
					if (ctriplet[My_position][ctriplet_num[My_position]] > comboLevel)
					{
						if (ctriplet[My_position][ctriplet_num[My_position]] >= ctriplet[0][ctriplet_num[0]] && ctriplet[0][ctriplet_num[0]] > comboLevel)
						{
							for (int j = 1; j <= ctriplet_num[My_position]; j++)
							{
								if (ctriplet[My_position][j] >= ctriplet[0][ctriplet_num[0]] && ctriplet[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::TRIPLET2,
										ctriplet[My_position][j], 2, cpair[My_position]);
								}
							}
						}
						else
						{
							for (int j = 1; j <= ctriplet_num[My_position]; j++)
							{
								if (ctriplet[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::TRIPLET2,
										ctriplet[My_position][j], 2, cpair[My_position]);
								}
							}
						}
					}
				}
				if (cbomb_num[My_position] > 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				if (rocket[My_position] == 1)
				{
					return CardCombo(CardComboType::ROCKET);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::PLANE)
			{
				if (packs.size() == 2)
				{
					if (cstraight3_2[My_position][cstraight3_2_num[My_position]] > comboLevel)
					{

						if (cstraight3_2[My_position][cstraight3_2_num[My_position]] >= cstraight3_2[0][cstraight3_2_num[0]] && cstraight3_2[0][cstraight3_2_num[0]] > comboLevel)
						{
							for (int j = 1; j <= cstraight3_2_num[My_position]; j++)
							{
								if (cstraight3_2[My_position][j] >= cstraight3_2[0][cstraight3_2_num[0]] && cstraight3_2[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE,
										cstraight3_2[My_position][j], 2);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight3_2_num[My_position]; j++)
							{
								if (cstraight3_2[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE,
										cstraight3_2[My_position][j], 2);
								}
							}
						}
					}
				}
				else if (packs.size() == 3)
				{
					if (cstraight3_3[My_position][cstraight3_3_num[My_position]] > comboLevel)
					{

						if (cstraight3_3[My_position][cstraight3_3_num[My_position]] >= cstraight3_3[0][cstraight3_3_num[0]] && cstraight3_3[0][cstraight3_3_num[0]] > comboLevel)
						{
							for (int j = 1; j <= cstraight3_3_num[My_position]; j++)
							{
								if (cstraight3_3[My_position][j] >= cstraight3_3[0][cstraight3_3_num[0]] && cstraight3_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE,
										cstraight3_3[My_position][j], 3);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight3_3_num[My_position]; j++)
							{
								if (cstraight3_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE,
										cstraight3_3[My_position][j], 3);
								}
							}
						}
					}
				}
				else if (packs.size() == 4)
				{
					if (cstraight3_4[My_position][cstraight3_4_num[My_position]] > comboLevel)
					{

						if (cstraight3_4[My_position][cstraight3_4_num[My_position]] >= cstraight3_4[0][cstraight3_4_num[0]] && cstraight3_4[0][cstraight3_4_num[0]] > comboLevel)
						{
							for (int j = 1; j <= cstraight3_4_num[My_position]; j++)
							{
								if (cstraight3_4[My_position][j] >= cstraight3_4[0][cstraight3_4_num[0]] && cstraight3_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE,
										cstraight3_4[My_position][j], 4);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight3_4_num[My_position]; j++)
							{
								if (cstraight3_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE,
										cstraight3_4[My_position][j], 4);
								}
							}
						}
					}
				}
				if (cbomb_num[My_position] > 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				if (rocket[My_position] == 1)
				{
					return CardCombo(CardComboType::ROCKET);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::PLANE1)
			{
				if (packs.size() / 2 == 2)
				{
					if (cstraight3_2[My_position][cstraight3_2_num[My_position]] > comboLevel && csingle_num[My_position] > packs.size() / 2)
					{

						if (cstraight3_2[My_position][cstraight3_2_num[My_position]] >= cstraight3_2[0][cstraight3_2_num[0]] && cstraight3_2[0][cstraight3_2_num[0]] > comboLevel)
						{
							for (int j = 1; j <= cstraight3_2_num[My_position]; j++)
							{
								if (cstraight3_2[My_position][j] >= cstraight3_2[0][cstraight3_2_num[0]] && cstraight3_2[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE1,
										cstraight3_2[My_position][j], 2, csingle[My_position]);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight3_2_num[My_position]; j++)
							{
								if (cstraight3_2[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE1,
										cstraight3_2[My_position][j], 2, csingle[My_position]);
								}
							}
						}
					}
				}
				else if (packs.size() / 2 == 3)
				{
					if (cstraight3_3[My_position][cstraight3_3_num[My_position]] > comboLevel && csingle_num[My_position] > packs.size() / 2)
					{

						if (cstraight3_3[My_position][cstraight3_3_num[My_position]] >= cstraight3_3[0][cstraight3_3_num[0]] && cstraight3_3[0][cstraight3_3_num[0]] > comboLevel)
						{
							for (int j = 1; j <= cstraight3_3_num[My_position]; j++)
							{
								if (cstraight3_3[My_position][j] >= cstraight3_3[0][cstraight3_3_num[0]] && cstraight3_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE1,
										cstraight3_3[My_position][j], 3, csingle[My_position]);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight3_3_num[My_position]; j++)
							{
								if (cstraight3_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE1,
										cstraight3_3[My_position][j], 3, csingle[My_position]);
								}
							}
						}
					}
				}
				else if (packs.size() / 2 == 4)
				{
					if (cstraight3_4[My_position][cstraight3_4_num[My_position]] > comboLevel && csingle_num[My_position] > packs.size() / 2)
					{

						if (cstraight3_4[My_position][cstraight3_4_num[My_position]] >= cstraight3_4[0][cstraight3_4_num[0]] && cstraight3_4[0][cstraight3_4_num[0]] > comboLevel)
						{
							for (int j = 1; j <= cstraight3_4_num[My_position]; j++)
							{
								if (cstraight3_4[My_position][j] >= cstraight3_4[0][cstraight3_4_num[0]] && cstraight3_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE1,
										cstraight3_4[My_position][j], 4, csingle[My_position]);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight3_4_num[My_position]; j++)
							{
								if (cstraight3_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE1,
										cstraight3_4[My_position][j], 4, csingle[My_position]);
								}
							}
						}
					}
				}
				if (cbomb_num[My_position] > 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				if (rocket[My_position] == 1)
				{
					return CardCombo(CardComboType::ROCKET);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::PLANE2)
			{
				if (packs.size() / 2 == 2)
				{
					if (cstraight3_2[My_position][cstraight3_2_num[My_position]] > comboLevel && cpair_num[My_position] > packs.size() / 2)
					{

						if (cstraight3_2[My_position][cstraight3_2_num[My_position]] >= cstraight3_2[0][cstraight3_2_num[0]] && cstraight3_2[0][cstraight3_2_num[0]] > comboLevel)
						{
							for (int j = 1; j <= cstraight3_2_num[My_position]; j++)
							{
								if (cstraight3_2[My_position][j] >= cstraight3_2[0][cstraight3_2_num[0]] && cstraight3_2[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE2,
										cstraight3_2[My_position][j], 2, cpair[My_position]);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight3_2_num[My_position]; j++)
							{
								if (cstraight3_2[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE2,
										cstraight3_2[My_position][j], 2, cpair[My_position]);
								}
							}
						}
					}
				}
				else if (packs.size() / 2 == 3)
				{
					if (cstraight3_3[My_position][cstraight3_3_num[My_position]] > comboLevel && cpair_num[My_position] > packs.size() / 2)
					{

						if (cstraight3_3[My_position][cstraight3_3_num[My_position]] >= cstraight3_3[0][cstraight3_3_num[0]] && cstraight3_3[0][cstraight3_3_num[0]] > comboLevel)
						{
							for (int j = 1; j <= cstraight3_3_num[My_position]; j++)
							{
								if (cstraight3_3[My_position][j] >= cstraight3_3[0][cstraight3_3_num[0]] && cstraight3_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE2,
										cstraight3_3[My_position][j], 3, cpair[My_position]);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight3_3_num[My_position]; j++)
							{
								if (cstraight3_3[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE2,
										cstraight3_3[My_position][j], 3, cpair[My_position]);
								}
							}
						}
					}
				}
				else if (packs.size() / 2 == 4)
				{
					if (cstraight3_4[My_position][cstraight3_4_num[My_position]] > comboLevel && cpair_num[My_position] > packs.size() / 2)
					{

						if (cstraight3_4[My_position][cstraight3_4_num[My_position]] >= cstraight3_4[0][cstraight3_4_num[0]] && cstraight3_4[0][cstraight3_4_num[0]] > comboLevel)
						{
							for (int j = 1; j <= cstraight3_4_num[My_position]; j++)
							{
								if (cstraight3_4[My_position][j] >= cstraight3_4[0][cstraight3_4_num[0]] && cstraight3_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE2,
										cstraight3_4[My_position][j], 4);
								}
							}
						}

						else
						{
							for (int j = 1; j <= cstraight3_4_num[My_position]; j++)
							{
								if (cstraight3_4[My_position][j] > comboLevel)
								{
									return CardCombo(CardComboType::PLANE2,
										cstraight3_4[My_position][j], 4);
								}
							}
						}
					}
				}
				if (cbomb_num[My_position] > 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				if (rocket[My_position] == 1)
				{
					return CardCombo(CardComboType::ROCKET);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::QUADRUPLE2)
			{
				if (cbomb_num[My_position] > 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::QUADRUPLE4)
			{
				if (cbomb_num[My_position] > 0)
				{
					return CardCombo(CardComboType::BOMB,
						cbomb[My_position][1], 1);
				}
				return CardCombo(CardComboType::PASS);
			}
			else if (comboType == CardComboType::ROCKET)
			{
				return CardCombo(CardComboType::PASS);
			}
		}
	}
	//特判

	//决策


}
/* 状态 */
