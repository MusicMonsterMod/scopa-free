#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace scopa {
using Cards = std::vector<int>;
using Capture = std::vector<int>; // Indices in the table, always sorted.
inline int rank(int c) { return c % 10 + 1; }
inline int suit(int c) { return c / 10; }
inline int primeValue(int c) { int r=rank(c); return r==7?21:r==6?18:r==1?16:r>=8?10:10+r; }
inline std::string name(int c) {
    static const char* suits[]={"Cups","Coins","Batons","Swords"};
    static const char* ranks[]={"Ace","2","3","4","5","6","7","Fante (8)","Cavallo (9)","King (10)"};
    return std::string(ranks[rank(c)-1])+" of "+suits[suit(c)];
}
inline void subsets(const Cards& table,int start,int remaining,Capture& current,std::vector<Capture>& out) {
    if(!remaining) { if(!current.empty()) out.push_back(current); return; }
    for(int i=start;i<(int)table.size();++i) if(rank(table[i])<=remaining) {
        current.push_back(i); subsets(table,i+1,remaining-rank(table[i]),current,out); current.pop_back();
    }
}
inline std::vector<Capture> captures(int card,const Cards& table,bool allowAll=false,bool aces=false) {
    std::vector<Capture> out;
    for(int i=0;i<(int)table.size();++i) if(rank(table[i])==rank(card)) out.push_back({i});
    if(aces && rank(card)==1 && out.empty() && !table.empty()) {
        Capture all(table.size());std::iota(all.begin(),all.end(),0);return {all};
    }
    if(out.empty()||allowAll) { Capture current; subsets(table,0,rank(card),current,out); }
    std::sort(out.begin(),out.end());out.erase(std::unique(out.begin(),out.end()),out.end());
    return out;
}
struct Rules { bool sweeps=true,allCaptures=false,aces=false,napoli=false,reBello=false,cappotto=false; int target=11,cappottoPoints=7; };
struct Breakdown { int cards=0,coins=0,seven=0,prime=0,sweeps=0,points=0,napoli=0,king=0; };
inline Breakdown tally(const Cards& cards,int sweeps) {
    Breakdown b; b.cards=cards.size(); b.sweeps=sweeps; std::array<int,4> best{};
    for(int c:cards) { b.coins+=suit(c)==1; b.seven+=c==16; b.king+=c==19; best[suit(c)]=std::max(best[suit(c)],primeValue(c)); }
    if(std::all_of(best.begin(),best.end(),[](int n){return n>0;})) b.prime=std::accumulate(best.begin(),best.end(),0);
    for(int c=10;c<20&&std::find(cards.begin(),cards.end(),c)!=cards.end();++c)++b.napoli;
    if(b.napoli<3)b.napoli=0;
    return b;
}
struct Move { int hand=0; Capture take; };
struct Game {
    Rules rules;
    Cards deck,table,hand[2],taken[2];
    int sweeps[2]{},total[2]{},turn=0,dealer=1,last=-1,round=0;
    uint32_t seed=1; bool over=false,finished=false; Breakdown score[2];
    std::mt19937 rng; std::string message;
    explicit Game(uint32_t s=1) { reset(s); }
    int draw() { int c=deck.back();deck.pop_back();return c; }
    void deal() { for(int i=0;i<3;++i) {hand[1-dealer].push_back(draw());hand[dealer].push_back(draw());} }
    void reset(uint32_t s) { seed=s; rng.seed(s); total[0]=total[1]=0; score[0]={};score[1]={}; round=0; dealer=1; finished=false; nextRound(); }
    void nextRound() {
        if(finished) return;
        if(round) dealer=1-dealer;
        ++round;over=false;last=-1;turn=1-dealer; sweeps[0]=sweeps[1]=0;
        taken[0].clear();taken[1].clear();
        do {
            deck.resize(40);std::iota(deck.begin(),deck.end(),0); std::shuffle(deck.begin(),deck.end(),rng);
            hand[0].clear();hand[1].clear();table.clear(); deal();
            for(int i=0;i<4;++i)table.push_back(draw());
        } while(std::count_if(table.begin(),table.end(),[](int c){return rank(c)==10;})>=3);
        message="Round "+std::to_string(round)+". "+(turn==0?"Your turn.":"Computer leads.");
    }
    void finishRound() {
        if(last>=0) { taken[last].insert(taken[last].end(),table.begin(),table.end());table.clear(); }
        for(int p=0;p<2;++p) score[p]=tally(taken[p],sweeps[p]);
        for(int p=0;p<2;++p) {
            auto& a=score[p];auto& b=score[1-p];
            a.points=(rules.sweeps?a.sweeps:0)+(rules.napoli?a.napoli:0)+(rules.reBello?a.king:0)+a.seven+(a.cards>b.cards)+(a.coins>b.coins)+(a.prime>b.prime);
            if(rules.cappotto&&(a.cards==40||b.cards==40))a.points=a.cards==40?rules.cappottoPoints:0;
            total[p]+=a.points;
        }
        over=true; finished=std::max(total[0],total[1])>=rules.target && total[0]!=total[1];
    }
    void play(int player,const Move& move) {
        if(over || player!=turn || move.hand<0 || move.hand>=(int)hand[player].size()) throw std::logic_error("Invalid turn");
        int c=hand[player][move.hand]; auto choices=legalCaptures(c);
        if((choices.empty()&&!move.take.empty()) || (!choices.empty()&&std::find(choices.begin(),choices.end(),move.take)==choices.end())) throw std::logic_error("Illegal capture");
        hand[player].erase(hand[player].begin()+move.hand);
        message=(player?"Computer: ":"You: ")+name(c);
        if(move.take.empty()) { table.push_back(c);message+=" placed."; }
        else {
            taken[player].push_back(c);
            for(auto i=move.take.rbegin();i!=move.take.rend();++i) {taken[player].push_back(table[*i]);table.erase(table.begin()+*i);}
            last=player;message+=" takes "+std::to_string(move.take.size())+".";
            if(table.empty() && !(rules.aces&&rank(c)==1) && !(deck.empty()&&hand[0].empty()&&hand[1].empty())) {++sweeps[player];message+=" SCOPA!";}
        }
        turn=1-player;
        if(hand[0].empty()&&hand[1].empty()) { if(deck.empty())finishRound(); else deal(); }
    }
    std::vector<Capture> legalCaptures(int c) const {return captures(c,table,rules.allCaptures,rules.aces);}
    Move bestMove(int player,int difficulty=2) {
        std::vector<Move> moves;
        for(int i=0;i<(int)hand[player].size();++i) {
            auto choices=legalCaptures(hand[player][i]);
            if(choices.empty())moves.push_back({i,{}});else for(auto& c:choices)moves.push_back({i,c});
        }
        if(moves.empty())throw std::logic_error("No legal moves");
        if(difficulty==0) return moves[rng()%moves.size()];
        double best=-1e9;Move chosen=moves[0];
        for(auto& m:moves) {
            int card=hand[player][m.hand]; Cards gain,remaining=table;
            double value=0;
            if(!m.take.empty()) {
                gain.push_back(card);
                for(auto i=m.take.rbegin();i!=m.take.rend();++i) {gain.push_back(table[*i]);remaining.erase(remaining.begin()+*i);}
                if(remaining.empty())value+=28;
                for(int c:gain) value+=2+(suit(c)==1?2:0)+(c==16?26:0)+(rank(c)==7?5:rank(c)==6?2:0);
            } else {remaining.push_back(card);value-=primeValue(card)*0.08+(card==16?20:0);}
            if(difficulty>=2) {
                // Only public information and our own hand are used; never inspect the opponent's hand.
                int sum=0;for(int c:remaining)sum+=rank(c);
                if(sum>0&&sum<=10)value-=18;
                for(int c:remaining)if(c==16)value-=9;
            }
            if(value>best) {best=value;chosen=m;}
        }
        return chosen;
    }
};
}
