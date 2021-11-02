#include <bits/stdc++.h>
using namespace std;
mt19937 rng(19260817);
uniform_real_distribution<double> real01(0,1);
float rndDouble(double l, double r){
	return l + (r-l) * real01(rng);
}
class Agent{
public:
	string atype; // full / mixed
	bool useHeu,useInf;
	double c, SoD_B, SoD_E; // MCTS
	double iW_B, iW_E; // utility func in MCTS
	double sW, pW; // design of utility func
	double u2P_E, infTL; // inference
	bool def0; // MCTS
	int thr;

	int score,count;
	Agent(): score(0), count(0){
		if(rng() % 3 == 0)
			atype = "mixed";
		else
			atype = "full";
		
		useHeu = rng() % 2; // full only
		useInf = rng() % 3;
		c = exp(rndDouble(-0.5,2.5));
		SoD_B = exp(rndDouble(1,5));
		SoD_E = exp(rndDouble(-5.5,0.5));
		iW_B = exp(rndDouble(-8,-3)); // full only, heu only
		iW_E = exp(rndDouble(-8,0)); // full only, heu only
		sW = exp(rndDouble(1,2.25)); // heu or mixed
		pW = exp(rndDouble(-1,1)); // heu or mixed
		u2P_E = exp(rndDouble(2.5,8.5)); // inf only
		infTL = exp(rndDouble(-3,-0.5)); // inf only
		def0 = (rng() % 3 == 0); // n-heu or mixed
		thr = 5 + rng() % 7; // mixed only

		if(!useInf) infTL = .15;
	}
	Agent(const Agent &pa,const Agent &ma): score(0), count(0){
		assert(pa.atype == ma.atype);
		const Agent *par[2]={&pa,&ma};

		atype = par[0]->atype;

		int heuInherit = rng()%2;
		useHeu = par[heuInherit]->useHeu;
		
		int infInherit = rng()%2;
		useInf = par[infInherit]->useInf;
		
		c = par[rng()%2]->c;
		SoD_B = par[rng()%2]->SoD_B;
		SoD_E = par[rng()%2]->SoD_E;

		if(atype == "full" && useHeu){
			bool otherOK = par[!heuInherit]->useHeu; 

			iW_B = par[heuInherit]->iW_B;
			if(rng()%2 && otherOK)
				iW_B = par[!heuInherit]->iW_B;
			
			iW_E = par[heuInherit]->iW_E;
			if(rng()%2 && otherOK)
				iW_E = par[!heuInherit]->iW_E;
		}

		if(atype == "mixed" || useHeu){
			bool otherOK = (par[!heuInherit]->atype == "mixed" || par[!heuInherit]->useHeu);

			sW = par[heuInherit]->sW;
			if(rng()%2 && otherOK)
				sW = par[!heuInherit]->sW;
			
			pW = par[heuInherit]->pW;
			if(rng()%2 && otherOK)
				pW = par[!heuInherit]->pW;
		}

		if(useInf){
			bool otherOK = par[!infInherit]->useInf;

			u2P_E = par[infInherit]->u2P_E;
			if(rng()%2 && otherOK)
				u2P_E = par[!infInherit]->u2P_E;
		}
		else infTL = .15;

		if(atype == "mixed" || !useHeu){
			bool otherOK = (par[!heuInherit]->atype == "mixed" || !(par[!heuInherit]->useHeu));

			def0 = par[heuInherit]->def0;
			if(rng()%2 && otherOK)
				def0 = par[!heuInherit]->def0;
		}

		thr = par[rng()%2]->thr;
	}
	string toStr(){
		if(!useInf) infTL = .15;
		auto concat = [](vector < string > vec){
			string ret="";
			for(auto &s : vec) ret += s + " ";
			return ret;
		};
		#define S to_string
		if(atype == "mixed")
			return concat({atype,S(useInf),S(c),S(SoD_B),S(SoD_E),S(sW),S(pW),S(u2P_E),S(infTL),S(def0),S(thr)});
		
		return concat({atype,S(useHeu),S(useInf),S(c),S(SoD_B),S(SoD_E),S(iW_B),S(iW_E),S(sW),S(pW),S(u2P_E),S(infTL),S(def0)});
		#undef S
	}
	void readPart(ifstream &IN,bool iniCS=1){
		IN >> atype;
		if(atype == "mixed"){
			IN >> useInf >> c >> SoD_B >> SoD_E >> sW >> pW >> u2P_E >> infTL >> def0 >> thr;
		}
		else{
			assert(atype == "full");
			IN >> useHeu >> useInf >> c >> SoD_B >> SoD_E >> iW_B >> iW_E >> sW >> pW >> u2P_E >> infTL >> def0;
		}
		if(iniCS) count = score = 0;
	}
	void readAll(ifstream &IN){
		IN >> score >> count;
		char tmp;
		IN >> tmp;
		assert(tmp=='|');
		readPart(IN,0);
	}
};
constexpr int NP=28,RPI=84; // 4|NP|RPI
vector < Agent > population(NP);

int battleCnt;
template < typename IT >
void process4(IT bg, IT ed){
	printf("[%d,%d)-th battle start.\n",battleCnt,battleCnt+4);
	battleCnt += 4;

	vector < vector < int > > pars(bg,ed);
	assert((int)pars.size()==4);

	for(int i=0;i<4;++i){
		const vector < int > &ids = pars[i];
		ofstream gameParticipants("simu" + to_string(i+1) + "/game-participants.txt",ios::out);
		for(int id : ids){
			gameParticipants << population[id].toStr() << endl;
		}
		gameParticipants.close();
	}

	system("./launch-games");

	for(int i=0;i<4;++i){
		const vector < int > &ids = pars[i];
		ifstream gameScores("simu" + to_string(i+1) + "/game-scores.txt",ios::in);
		int scores[3];
		for(int j=0;j<3;++j){
			gameScores >> scores[j];
			population[ids[j]].score += scores[j];
		}
		gameScores.close();
	}
}

int weightedRandint(const vector < int > &w){
	int sum = 0;
	for(int v:w) sum += v;
	int t = rng()%sum;
	for(int i=0;i<(int)w.size()-1;++i){
		t -= w[i];
		if(t<0) return i;
	}
	return (int)w.size()-1;
}

int main(){
	for(int iter=0;;++iter){

		printf("\n\n%d-th iteration start.\n\n\n",iter);

		{
			ifstream readEnd("state-end-" + to_string(iter) + ".txt", ios::in);
			if(readEnd.good()){
				printf("skip to end.\n");
				for(Agent &A : population){
					A.readAll(readEnd);
				}
				readEnd.close();
				goto afterEnd;
			}
			readEnd.close();
		}

		{
			ifstream readMid("state-mid-" + to_string(iter) + ".txt", ios::in);
			if(readMid.good()){
				printf("skip to mid.\n");
				for(Agent &A : population){
					A.readAll(readMid);
				}
				readMid.close();
				goto afterMid;
			}
			readMid.close();
		}

		{
			ifstream readBegin("state-begin-" + to_string(iter) + ".txt", ios::in);
			if(readBegin.good()){
				printf("skip to begin.\n");
				for(Agent &A : population){
					A.readPart(readBegin);
				}
				readBegin.close();
				goto afterBegin;
			}
			readBegin.close();
		}

		{
			ofstream loggerBegin("state-begin-" + to_string(iter) + ".txt", ios::out);
			for(Agent A : population){
				loggerBegin << A.toStr() << endl;
			}
			loggerBegin.close();
		}

	afterBegin:
		{
			assert((int)population.size() == NP);
			vector < vector < int > > tuples;
			vector < int > per(NP);
			for(int i=0;i<NP;++i) per[i]=i;
			for(int i=0;i<RPI;++i){
				shuffle(per.begin(),per.end(),rng);
				stable_sort(per.begin(),per.end(),[&](int a,int b){return population[a].count<population[b].count;});
				tuples.emplace_back();
				for(int j=0;j<3;++j){
					++population[per[j]].count;
					tuples.back().emplace_back(per[j]);
				}
				shuffle(tuples.back().begin(),tuples.back().end(),rng);
			}
			assert((int)tuples.size()==RPI);
			for(int i=0;i<(int)tuples.size();i+=4){
				process4(tuples.begin()+i,tuples.begin()+i+4);
			}

			for(int i=1;i<NP;++i){
				assert(population[i].count == population[0].count);
			}
			sort(population.begin(),population.end(),[](Agent a,Agent b){return a.score > b.score;});
		}

		{
			ofstream loggerMid("state-mid-" + to_string(iter) + ".txt", ios::out);
			for(Agent A : population){
				loggerMid << A.score << "\t" << A.count << "\t|  " << A.toStr() << endl;
			}
			loggerMid.close();
		}

	afterMid:
		{
			if(population[10].score <= 0){
				int dlt = -population[10].score + 1;
				for(Agent &A : population){
					A.score += dlt;
				}
			}
			while(population.back().score <= 0) population.pop_back();
			vector < int > weights(population.size());
			for(int i=0;i<(int)weights.size();++i){
				weights[i] = population[i].score;
			}
			while((int)population.size() < NP){
				int x=weightedRandint(weights),
					y=weightedRandint(weights);
				while(x==y || population[x].atype!=population[y].atype){
					x=weightedRandint(weights);
					y=weightedRandint(weights);
				}
				population.emplace_back(population[x],population[y]);
			}
		}

		{
			ofstream loggerEnd("state-end-" + to_string(iter) + ".txt", ios::out);
			for(Agent A : population){
				loggerEnd << A.score << "\t" << A.count << "\t|  " << A.toStr() << endl;
			}
			loggerEnd.close();
		}

	afterEnd:
		for(Agent &A : population){
			A.count = A.score = 0;
		}
	}
	return 0;
}