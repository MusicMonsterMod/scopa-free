#include "game.hpp"
#include <iostream>
#include <set>
using namespace scopa;
void check(bool b,const char* message) {if(!b)throw std::runtime_error(message);}
void conservation(const Game& g) {
    Cards all=g.deck;all.insert(all.end(),g.table.begin(),g.table.end());
    for(int p=0;p<2;++p) {all.insert(all.end(),g.hand[p].begin(),g.hand[p].end());all.insert(all.end(),g.taken[p].begin(),g.taken[p].end());}
    check(all.size()==40&&std::set<int>(all.begin(),all.end()).size()==40,"card conservation");
}
int main() {
    check(captures(4,{0,3,14})==std::vector<Capture>{{2}},"single card precedence");
    check(captures(4,{0,3,10,13})==std::vector<Capture>({{0,1},{0,3},{1,2},{2,3}}),"all sum choices");
    check(captures(1,{4,9}).empty(),"no capture");
    check(tally({6,16,26,36},0).prime==84,"primiera sevens");
    check(tally({6,16,26},0).prime==0,"primiera needs four suits");
    Game g(42);auto start=g.deck;g.reset(42);check(g.deck==start,"seed replay");
    g.deck.clear();g.table={0};g.hand[0]={10};g.hand[1].clear();g.turn=0;g.play(0,{0,{0}});
    check(g.sweeps[0]==0&&g.over,"last move not scopa");
    g.reset(42);g.table={0};g.hand[0]={10};g.hand[1]={5};g.turn=0;g.play(0,{0,{0}});check(g.sweeps[0]==1,"normal sweep");
    g.reset(42);g.deck.clear();g.table={9};g.hand[0]={0};g.hand[1].clear();g.turn=0;g.last=1;g.taken[0].clear();g.taken[1].clear();g.play(0,{0,{}});
    check(g.taken[1]==Cards({9,0})&&g.table.empty(),"last capturer gets leftovers");
    g.reset(42);g.table.clear();g.taken[0].clear();g.taken[1].clear();
    for(int c=0;c<40;++c)g.taken[(c%10)%2].push_back(c);
    g.sweeps[0]=2;g.sweeps[1]=1;g.finishRound();
    check(g.score[0].cards==20&&g.score[1].cards==20,"equal card totals");
    check(g.score[0].coins==5&&g.score[1].coins==5,"equal coin totals");
    check(g.score[0].points==4&&g.score[1].points==1,"tied majorities award neither; seven and prime score");
    Game a(100),b(100);b.hand[1]={0,1,2};
    auto ma=a.bestMove(0),mb=b.bestMove(0);
    check(ma.hand==mb.hand&&ma.take==mb.take,"AI does not inspect hidden opponent hand");
    check(captures(4,{0,3,14},true)==std::vector<Capture>({{0,1},{2}}),"allow sums alongside equal cards");
    check(captures(0,{3,5,9},false,true)==std::vector<Capture>({{0,1,2}}),"ace captures table");
    check(captures(0,{10,3,5},false,true)==std::vector<Capture>({{0}}),"matching ace takes priority");
    check(tally({10,11,12,13,19},0).napoli==4,"consecutive Napoli coins");
    check(tally({10,12,13},0).napoli==0,"Napoli requires ace two three");
    g.reset(42);g.rules.aces=true;g.table={3,5};g.hand[0]={0};g.hand[1]={9};g.turn=0;
    g.play(0,{0,{0,1}});check(g.sweeps[0]==0,"ace take-all is not a sweep");
    g.reset(42);g.rules.cappotto=true;g.rules.cappottoPoints=7;g.table.clear();g.taken[0].resize(40);std::iota(g.taken[0].begin(),g.taken[0].end(),0);g.taken[1].clear();g.finishRound();
    check(g.score[0].points==7&&g.score[1].points==0,"Cappotto replaces ordinary scoring");
    for(unsigned seed=1;seed<=500;++seed) {
        Game a(seed);int turns=0;
        while(!a.finished) {
            while(!a.over) {conservation(a);a.play(a.turn,a.bestMove(a.turn,seed%3));check(++turns<3600,"match terminates");}
            conservation(a);check(a.taken[0].size()+a.taken[1].size()==40,"all captured");
            check(a.score[0].seven+a.score[1].seven==1,"one settebello");
            if(!a.finished)a.nextRound();
        }
        check(a.total[0]!=a.total[1],"no tied match winner");
    }
    std::cout<<"PASS: capture rules, scoring, sweep exception, replay, and 500 complete matches\n";
}
