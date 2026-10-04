#include "game.hpp"
#include <SDL.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
using namespace scopa;
namespace fs=std::filesystem;
struct Color {Uint8 r,g,b;};
constexpr Color teal{0,128,128},white{255,255,255},black{0,0,0},face{245,245,245},shadow{128,128,128},light{255,255,255},blue{0,0,128};
constexpr int kCanvasW=792,kCanvasH=566,kCardW=71,kCardH=125;
struct Hit {SDL_Rect r;std::function<void()> fn;};
enum class Dialog {None,Options,Selection,Score,Rules,About,Statistics,Error};
class App {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;SDL_Texture* font=nullptr;
    SDL_Texture* cards[40]{},*negative[40]{},*backs[8]{},*empty=nullptr,*dashed=nullptr,*foundation=nullptr,*menubar=nullptr;
    int outputW=kCanvasW,outputH=kCanvasH,windowW=kCanvasW,windowH=kCanvasH;
    // Card bitmaps stay 71×125; only gaps grow when the window grows (classic Solitaire).
    int tableLeft=40,tableY=249,tableStepX=80,tableStepY=22;
    int handLeft=280,handStep=80,handCompY=74,handPlayerY=434;
    int stockX=150,pileL=25,pileR=695,pileY=37;
    std::array<Uint8,256> widths{};
    SDL_AudioDeviceID audio=0;std::vector<Uint8> sounds[2];
    fs::path assets;Game game;bool running=true,sound=true,faceUp=true,dealt=false,waitingDeal=false;
    int selected=-1,choice=0,difficulty=0,back=0,menu=0,menuItem=0,hoverX=-1,hoverY=-1;
    int handSlots[2][3]{{-1,-1,-1},{-1,-1,-1}};
    std::array<int,40> tableSlots;
    Capture chosen;std::vector<Capture> choices;
    std::vector<Hit> hits;Uint32 aiAt=0;Dialog dialog=Dialog::None;
    std::string error,seedText,pointsText,cappottoText,screenshot;int edit=0;
    int gamesPlayed=0,gamesWon=0;bool counted=false,scoreContinues=false;
    Rules draft;int draftBack=0,draftDifficulty=0;bool draftSound=true,draftFaceUp=true;
    void color(Color c){SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,255);}
    void fill(SDL_Rect r,Color c){color(c);SDL_RenderFillRect(renderer,&r);}
    void line(int x,int y,int xx,int yy,Color c){color(c);SDL_RenderDrawLine(renderer,x,y,xx,yy);}
    void border(SDL_Rect r,Color c){color(c);SDL_RenderDrawRect(renderer,&r);}
    int textWidth(const std::string& s){int n=0;for(unsigned char c:s)if(c!='&')n+=widths[c];return n;}
    void text(int x,int y,const std::string& s,Color c=black,bool mnemonic=false){
        SDL_SetTextureColorMod(font,c.r,c.g,c.b);bool underline=false;
        for(unsigned char ch:s){
            if(mnemonic&&ch=='&'){underline=true;continue;}
            SDL_Rect src{(ch%16)*24,(ch/16)*16,widths[ch],13},dst{x,y,widths[ch],13};
            SDL_RenderCopy(renderer,font,&src,&dst);
            if(underline){line(x,y+11,x+widths[ch]-2,y+11,c);underline=false;}
            x+=widths[ch];
        }
    }
    void bevel(SDL_Rect r,bool down=false){
        fill(r,face);Color a=down?shadow:light,b=down?light:shadow;
        line(r.x,r.y,r.x+r.w-1,r.y,a);line(r.x,r.y,r.x,r.y+r.h-1,a);
        line(r.x,r.y+r.h-1,r.x+r.w-1,r.y+r.h-1,b);line(r.x+r.w-1,r.y,r.x+r.w-1,r.y+r.h-1,b);
        if(!down){line(r.x+1,r.y+r.h-2,r.x+r.w-2,r.y+r.h-2,black);line(r.x+r.w-2,r.y+1,r.x+r.w-2,r.y+r.h-2,black);}
    }
    void hit(SDL_Rect r,std::function<void()> fn){hits.push_back({r,fn});}
    void button(SDL_Rect r,const std::string& label,std::function<void()> fn,bool focus=false){
        bevel(r);text(r.x+(r.w-textWidth(label))/2,r.y+(r.h-13)/2,label);
        if(focus)for(int x=r.x+3;x<r.x+r.w-3;x+=2){line(x,r.y+3,x,r.y+3,black);line(x,r.y+r.h-4,x,r.y+r.h-4,black);}
        hit(r,fn);
    }
    SDL_Texture* load(const std::string& file,bool key=false,bool invert=false){
        SDL_Surface* s=SDL_LoadBMP((assets/file).c_str());if(!s)throw std::runtime_error("Cannot load "+file+": "+SDL_GetError());
        if(invert){auto converted=SDL_ConvertSurfaceFormat(s,SDL_PIXELFORMAT_ARGB8888,0);SDL_FreeSurface(s);s=converted;
            for(int y=0;y<s->h;++y){auto row=reinterpret_cast<Uint32*>(static_cast<Uint8*>(s->pixels)+y*s->pitch);for(int x=0;x<s->w;++x)row[x]^=0x00ffffff;}}
        if(key)SDL_SetColorKey(s,SDL_TRUE,SDL_MapRGB(s->format,0,0,0));
        auto t=SDL_CreateTextureFromSurface(renderer,s);SDL_FreeSurface(s);if(!t)throw std::runtime_error(SDL_GetError());
        SDL_SetTextureScaleMode(t,SDL_ScaleModeNearest);return t;
    }
    void bitmap(SDL_Texture* t,int x,int y,int w=kCardW,int h=kCardH){SDL_Rect r{x,y,w,h};SDL_RenderCopy(renderer,t,nullptr,&r);}
    void playSound(int i){if(audio&&sound){SDL_ClearQueuedAudio(audio);SDL_QueueAudio(audio,sounds[i].data(),sounds[i].size());SDL_PauseAudioDevice(audio,0);}}
    void clearSelection(){selected=-1;chosen.clear();choices.clear();choice=0;}
    void title(){SDL_SetWindowTitle(window,("Scopa Free For Windows (#"+std::to_string(game.seed)+")").c_str());}
    void clearBoard(){tableSlots.fill(-1);for(auto& row:handSlots)for(int& c:row)c=-1;}
    void newGame(uint32_t seed){game.reset(seed);dealt=false;waitingDeal=false;clearBoard();clearSelection();dialog=Dialog::None;menu=0;counted=false;title();}
    void sync(int preferred=-1){
        for(int p=0;p<2;++p){
            for(int& c:handSlots[p])if(std::find(game.hand[p].begin(),game.hand[p].end(),c)==game.hand[p].end())c=-1;
            for(int c:game.hand[p])if(std::find(std::begin(handSlots[p]),std::end(handSlots[p]),c)==std::end(handSlots[p]))
                for(int& slot:handSlots[p])if(slot<0){slot=c;break;}
        }
        for(int& c:tableSlots)if(std::find(game.table.begin(),game.table.end(),c)==game.table.end())c=-1;
        for(int c:game.table)if(std::find(tableSlots.begin(),tableSlots.end(),c)==tableSlots.end()){
            if(preferred>=0&&tableSlots[preferred]<0){tableSlots[preferred]=c;preferred=-1;}
            else for(int& slot:tableSlots)if(slot<0){slot=c;break;}
        }
    }
    void deal(){
        if(game.over||(!waitingDeal&&dealt))return;
        dealt=true;waitingDeal=false;sync();playSound(1);aiAt=SDL_GetTicks()+650;
    }
    void selectCard(int c){
        if(!dealt||waitingDeal||game.turn||game.over)return;
        if(selected==c){clearSelection();return;}
        selected=c;choices=game.legalCaptures(c);chosen.clear();choice=0;playSound(0);
    }
    void perform(const Capture& take,int preferred=-1){
        if(selected<0||game.turn||game.over)return;
        auto it=std::find(game.hand[0].begin(),game.hand[0].end(),selected);if(it==game.hand[0].end())return;
        if((choices.empty()&&!take.empty())||(!choices.empty()&&std::find(choices.begin(),choices.end(),take)==choices.end()))return;
        int stock=game.deck.size();game.play(0,{int(it-game.hand[0].begin()),take});
        if((int)game.deck.size()<stock)waitingDeal=true;
        clearSelection();sync(preferred);playSound(1);aiAt=SDL_GetTicks()+650;roundEnd();
    }
    void roundEnd(){if(!game.over)return;scoreContinues=true;dialog=Dialog::Score;
        if(game.finished&&!counted){++gamesPlayed;gamesWon+=game.total[0]>game.total[1];counted=true;}}
    void tableClick(int slot){
        if(selected<0||game.turn||!dealt||waitingDeal)return;
        int c=tableSlots[slot];
        if(choices.empty()){if(c<0)perform({},slot);return;}
        if(c<0){if(std::find(choices.begin(),choices.end(),chosen)!=choices.end())perform(chosen);return;}
        int index=std::find(game.table.begin(),game.table.end(),c)-game.table.begin();
        auto it=std::find(chosen.begin(),chosen.end(),index);if(it==chosen.end())chosen.push_back(index);else chosen.erase(it);
        std::sort(chosen.begin(),chosen.end());
        if(std::find(choices.begin(),choices.end(),chosen)!=choices.end()){perform(chosen);return;}
        // Clicking a single legal target captures immediately; combinations accumulate.
        bool prefix=false;for(auto& cap:choices)if(std::includes(cap.begin(),cap.end(),chosen.begin(),chosen.end()))prefix=true;
        if(!prefix){chosen.clear();playSound(0);}
    }
    void keyboardPlay(){if(selected<0)return;if(choices.empty())perform({});else perform(chosen.empty()?choices[choice]:chosen);}
    void hint(){if(!dealt||waitingDeal||game.over||game.turn)return;auto m=game.bestMove(0,2);selectCard(game.hand[0][m.hand]);chosen=m.take;}
    void continueScore(){dialog=Dialog::None;if(!scoreContinues)return;scoreContinues=false;
        if(game.finished)newGame(randomSeed());else{game.nextRound();dealt=false;waitingDeal=false;clearBoard();clearSelection();}}
    static uint32_t randomSeed(){auto n=uint32_t(std::chrono::system_clock::now().time_since_epoch().count());return n?n:1;}
    void refreshLayout(){
        SDL_GetWindowSize(window,&windowW,&windowH);
        SDL_GetRendererOutputSize(renderer,&outputW,&outputH);
        if(outputW<1)outputW=kCanvasW;
        if(outputH<1)outputH=kCanvasH;
        int side=std::clamp(40+(outputW-kCanvasW)/10,8,120);
        tableStepX=std::max(75,(outputW-2*side-kCardW)/8);
        tableLeft=side;
        handStep=tableStepX;
        handLeft=(outputW-(2*handStep+kCardW))/2;
        stockX=handLeft-130;
        if(stockX<side)stockX=side;
        pileL=tableLeft-15;
        if(pileL<8)pileL=8;
        pileR=tableLeft+8*tableStepX+15;
        if(pileR+kCardW>outputW-8)pileR=outputW-kCardW-8;
        pileY=37;
        int extraH=std::max(0,outputH-kCanvasH);
        handCompY=74;
        handPlayerY=outputH-132;
        if(handPlayerY<handCompY+kCardH+20+kCardH)handPlayerY=handCompY+kCardH+20+kCardH;
        tableY=handCompY+175+extraH*2/5;
        if(tableY+kCardH+12>handPlayerY)tableY=std::max(handCompY+kCardH+12,handPlayerY-kCardH-12);
        tableStepY=22+extraH/20;
        if(tableStepY>40)tableStepY=40;
    }
    SDL_Rect tableRect(int slot){return {tableLeft+(slot%9)*tableStepX,tableY+(slot/9)*tableStepY,kCardW,kCardH};}
    int handIndex(int p,int slot){auto it=std::find(game.hand[p].begin(),game.hand[p].end(),handSlots[p][slot]);return it==game.hand[p].end()?-1:int(it-game.hand[p].begin());}
    void openOptions(){draft=game.rules;draftBack=back;draftDifficulty=difficulty;draftSound=sound;draftFaceUp=faceUp;pointsText=std::to_string(draft.target);cappottoText=std::to_string(draft.cappottoPoints);edit=0;dialog=Dialog::Options;menu=0;}
    void openSelect(){seedText=std::to_string(game.seed);edit=1;dialog=Dialog::Selection;menu=0;SDL_StartTextInput();}
    void command(int item){menu=0;switch(item){case 0:newGame(randomSeed());break;case 1:openSelect();break;case 2:newGame(game.seed);break;case 3:openOptions();break;case 4:dialog=Dialog::Statistics;break;case 5:scoreContinues=false;dialog=Dialog::Score;break;case 6:running=false;break;}}
    void mainMenu(){
        fill({0,0,outputW,19},white);
        bitmap(menubar,0,0,kCanvasW,19);
        if(menu==1)fill({0,0,40,19},{49,106,197});
        if(menu==2)fill({40,0,39,19},{49,106,197});
        if(menu==1)text(6,3,"&Game",white,true);
        if(menu==2)text(46,3,"&Help",white,true);
        hit({0,0,40,19},[this]{menu=menu==1?0:1;menuItem=0;});hit({40,0,39,19},[this]{menu=menu==2?0:2;menuItem=0;});
    }
    void drawMenu(){
        if(!menu)return;
        int x=menu==1?0:40,w=menu==1?141:161,n=menu==1?7:2;
        SDL_Rect r{x,19,w,menu==1?134:42};fill(r,white);border(r,{160,160,160});
        const char* labels[]={"&New - Random","New - &Select","&Replay Game","&Options...","S&tatistics","&Game Score","E&xit"};
        const char* help[]={"Scopa Free &Help","&About Program"};
        for(int i=0;i<n;++i){int y=23+i*17+(menu==1&&i==6?9:0);bool hover=hoverX>=x+3&&hoverX<x+w-3&&hoverY>=y-1&&hoverY<y+16;
            if(hover){fill({x+3,y-1,w-6,17},{49,106,197});menuItem=i;}
            text(x+20,y,menu==1?labels[i]:help[i],hover?white:black,true);
            if(menu==1&&i<6)text(x+114,y,"F"+std::to_string(i+2),hover?white:black);
            if(menu==1&&i==5)line(x+4,y+18,x+w-5,y+18,{160,160,160});
            int which=menu;hit({x+3,y-1,w-6,17},[this,i,which]{if(which==1)command(i);else{menu=0;dialog=i?Dialog::About:Dialog::Rules;}});
        }
    }
    void pile(int p){int x=p?pileR:pileL;auto& v=game.taken[p];
        if(v.empty()){bitmap(foundation,x,pileY,kCardW,96);return;}
        int layers=std::min(8,int(v.size()));for(int i=0;i<layers;++i)bitmap(backs[back],x,pileY+i*2);
        if(faceUp)bitmap(cards[v.back()],x,pileY+(layers-1)*2);
    }
    void drawBoard(){
        refreshLayout();
        fill({0,0,outputW,outputH},teal);hits.clear();mainMenu();
        auto pileLabel=[this](int p){return std::string(p?"Comp Takes (":"Player Takes (")+std::to_string(game.taken[p].size())+(game.rules.sweeps?"-"+std::to_string(game.sweeps[p]):"")+")";};
        std::string leftLabel=pileLabel(0),rightLabel=pileLabel(1);
        text(std::max(8,pileL-5),19,leftLabel,white);
        text(std::min(outputW-textWidth(rightLabel)-8,pileR-9),19,rightLabel,white);
        text(handLeft+handStep,handCompY-18,"Computer Hand",white);
        text(handLeft+handStep,handPlayerY-18,"Player Hand",white);
        pile(0);pile(1);
        for(int i=0;i<3;++i){int x=handLeft+handStep*i;
            bitmap(empty,x,handCompY);bitmap(empty,x,handPlayerY);
            if(dealt&&!waitingDeal&&handSlots[1][i]>=0)bitmap(backs[back],x,handCompY);
            if(dealt&&!waitingDeal&&handSlots[0][i]>=0){int c=handSlots[0][i];bitmap(selected==c?negative[c]:cards[c],x,handPlayerY);}
            hit({x,handPlayerY,kCardW,kCardH},[this,i]{if(handIndex(0,i)>=0)selectCard(handSlots[0][i]);});
        }
        for(int i=0;i<9;++i){auto r=tableRect(i);bitmap(dashed,r.x,r.y);hit(r,[this,i]{tableClick(i);});}
        if(dealt){for(int i=0;i<40;++i)if(tableSlots[i]>=0){int c=tableSlots[i];int idx=std::find(game.table.begin(),game.table.end(),c)-game.table.begin();auto r=tableRect(i);
            bitmap(std::find(chosen.begin(),chosen.end(),idx)!=chosen.end()?negative[c]:cards[c],r.x,r.y);if(i>=9)hit(r,[this,i]{tableClick(i);});}}
        int stock=!dealt?40:game.deck.size()+(waitingDeal?6:0);
        std::string stockLabel=std::string("Stock ")+(stock<10?" ":"")+std::to_string(stock);
        text(stockX+12,handPlayerY-18,stockLabel,white);
        bitmap(stock?backs[back]:empty,stockX,handPlayerY);hit({stockX,handPlayerY,kCardW,kCardH},[this]{deal();});
        hit({pileL,pileY,kCardW,142},[this]{scoreContinues=false;dialog=Dialog::Score;});
        hit({pileR,pileY,kCardW,142},[this]{scoreContinues=false;dialog=Dialog::Score;});
        if(dialog!=Dialog::None)drawDialog();else drawMenu();
    }
    // Dialog dimensions and control positions follow the original Win32 resources.
    void group(int x,int y,int w,int h,const std::string& label){
        border({x,y+6,w,h-6},light);border({x,y+5,w-1,h-6},{160,160,160});fill({x+7,y,textWidth(label)+4,13},face);text(x+8,y,label);
    }
    void check(int x,int y,const std::string& label,bool checked,std::function<void()> fn,bool radio=false){
        int yy=y+3;fill({x,yy,12,12},white);
        if(radio){fill({x,yy,12,12},face);color(white);for(int row=1;row<11;++row){int a=(row==1||row==10)?3:(row==2||row==9)?1:0;line(x+a,yy+row,x+10-a,yy+row,white);}line(x+3,yy,x+7,yy,shadow);line(x,yy+3,x,yy+7,shadow);line(x+1,yy+1,x+2,yy+1,shadow);line(x+9,yy+3,x+9,yy+7,white);if(checked){fill({x+4,yy+4,3,3},black);}}
        else {line(x,yy,x+11,yy,shadow);line(x,yy,x,yy+11,shadow);line(x+1,yy+1,x+10,yy+1,{64,64,64});line(x+1,yy+1,x+1,yy+10,{64,64,64});if(checked){line(x+3,yy+5,x+5,yy+7,black);line(x+5,yy+7,x+9,yy+3,black);line(x+3,yy+6,x+5,yy+8,black);line(x+5,yy+8,x+9,yy+4,black);}}
        text(x+17,y+2,label);hit({x,y,textWidth(label)+23,18},fn);
    }
    void editBox(SDL_Rect r,const std::string& value,int which){
        fill(r,white);line(r.x,r.y,r.x+r.w-1,r.y,shadow);line(r.x,r.y,r.x,r.y+r.h-1,shadow);line(r.x,r.y+r.h-1,r.x+r.w-1,r.y+r.h-1,light);line(r.x+r.w-1,r.y,r.x+r.w-1,r.y+r.h-1,light);
        text(r.x+3,r.y+2,value);if(edit==which&&(SDL_GetTicks()/500)%2==0)line(r.x+3+textWidth(value),r.y+2,r.x+3+textWidth(value),r.y+14,black);
        hit(r,[this,which]{edit=which;SDL_StartTextInput();});
    }
    void options(){
        const int w=335,h=358,x=(outputW-w)/2,y=(outputH-h)/2;fill({x,y,w,h},face);
        group(x+12,y+15,126,206,"Card Back and Sound");group(x+155,y+15,163,206,"Game Scoring");
        group(x+12,y+229,126,78,"Computer Strength");group(x+155,y+229,163,78,"Game Rules");
        const char* names[]={"Plain Red","Plain Blue","Roses","Italy Flag","DaVinci","Boat on Water","Piza Tower","King"};
        for(int i=0;i<8;++i)check(x+32,y+31+i*19.5,names[i],draftBack==i,[this,i]{draftBack=i;},true);
        check(x+27,y+192,"Enable Sounds",draftSound,[this]{draftSound=!draftSound;});
        const char* levels[]={"Beginner","Advanced","Expert"};
        for(int i=0;i<3;++i)check(x+27,y+244+i*19.5,levels[i],draftDifficulty==i,[this,i]{draftDifficulty=i;},true);
        editBox({x+171,y+36,33,20},pointsText,2);text(x+210,y+39,"Game Points");
        check(x+171,y+63,"Score 1 for Sweeps",draft.sweeps,[this]{draft.sweeps=!draft.sweeps;});
        check(x+171,y+86,"Scopa D'Assi",draft.aces,[this]{draft.aces=!draft.aces;});
        check(x+171,y+109,"Score For Napoli",draft.napoli,[this]{draft.napoli=!draft.napoli;});
        check(x+171,y+132,"Score Re Bello",draft.reBello,[this]{draft.reBello=!draft.reBello;});
        check(x+171,y+154,"Allow Cappotto",draft.cappotto,[this]{draft.cappotto=!draft.cappotto;});editBox({x+272,y+154,33,20},cappottoText,3);
        check(x+171,y+250,"Show Captures Face Up",draftFaceUp,[this]{draftFaceUp=!draftFaceUp;});
        check(x+171,y+274,"Allow All Captures",draft.allCaptures,[this]{draft.allCaptures=!draft.allCaptures;});
        button({x+30,y+322,75,23},"OK",[this]{
            try{int target=std::stoi(pointsText),cap=std::stoi(cappottoText);if(target<1||target>999||cap<1||cap>99)throw std::runtime_error("range");draft.target=target;draft.cappottoPoints=cap;}
            catch(...){return;}
            game.rules=draft;back=draftBack;difficulty=draftDifficulty;sound=draftSound;faceUp=draftFaceUp;clearSelection();dialog=Dialog::None;SDL_StopTextInput();});
        button({x+129,y+322,75,23},"Help",[this]{dialog=Dialog::Rules;});button({x+228,y+322,75,23},"Cancel",[this]{dialog=Dialog::None;SDL_StopTextInput();});
    }
    void selection(){const int w=299,h=99,x=(outputW-w)/2,y=(outputH-h)/2;fill({x,y,w,h},face);text(x+32,y+8,"Enter a number between 1 and 4294967295");editBox({x+54,y+31,190,21},seedText,1);
        button({x+6,y+62,98,26},"Use This Game",[this]{useSeed(0);},true);button({x+110,y+62,69,26},"Next Game",[this]{useSeed(1);});button({x+185,y+62,107,26},"Use Random Game",[this]{SDL_StopTextInput();newGame(randomSeed());});}
    void useSeed(int add){try{size_t end=0;auto n=std::stoull(seedText,&end);if(end!=seedText.size()||n<1||n>UINT32_MAX)return;SDL_StopTextInput();newGame(uint32_t(n+add)?uint32_t(n+add):1);}catch(...) {}}
    void messageBox(const std::vector<std::string>& lines,const std::string& label,std::function<void()> action,int minWidth=300){
        int w=minWidth;for(auto& s:lines)w=std::max(w,textWidth(s)+44);int h=int(lines.size())*16+55,x=(outputW-w)/2,y=(outputH-h)/2;
        fill({x,y,w,h},face);int yy=y+12;for(auto& s:lines){text(x+22,yy,s);yy+=16;}button({x+(w-75)/2,y+h-32,75,23},label,action,true);
    }
    void scoreDialog(){auto a=game.score[0],b=game.score[1];
        auto winner=[](int x,int y){return std::string(x==y?"Nobody":x>y?"Player":"Computer");};
        std::vector<std::string> lines={scoreContinues?"This round of Scopa Free is finished...":"Scores for the previous round of Scopa Free...",scoreContinues?"Scoring for this round:":"Scores for the last round:","",
            "1 point for Most Cards ("+std::to_string(std::max(a.cards,b.cards))+") goes to "+winner(a.cards,b.cards),
            "1 point for Most Coins ("+std::to_string(std::max(a.coins,b.coins))+") goes to "+winner(a.coins,b.coins),
            "1 point for the Sette Bello goes to "+winner(a.seven,b.seven),
            "1 point for Primiera ("+std::to_string(std::max(a.prime,b.prime))+") goes to "+winner(a.prime,b.prime)};
        if(game.rules.sweeps)lines.push_back("Sweeps for Player="+std::to_string(a.sweeps)+" and for Computer="+std::to_string(b.sweeps));
        if(game.rules.reBello)lines.push_back("1 point for Re Bello goes to "+winner(a.king,b.king));
        if(game.rules.napoli)lines.push_back(std::to_string(std::max(a.napoli,b.napoli))+" points for Napoli goes to "+winner(a.napoli,b.napoli));
        if(game.rules.cappotto&&(a.cards==40||b.cards==40))lines.push_back(winner(a.cards,b.cards)+" has won Scopa Free - Cappotto!");
        lines.push_back("");lines.push_back("Totals for last round:  Player="+std::to_string(a.points)+"   Computer="+std::to_string(b.points));lines.push_back("");
        lines.push_back("Totals for Game:  Player="+std::to_string(game.total[0])+"   Computer="+std::to_string(game.total[1]));
        if(game.finished)lines.push_back(game.total[0]>game.total[1]?"Player has won Scopa Free!":"Computer has won Scopa Free!");
        messageBox(lines,"OK",[this]{continueScore();});
    }
    void drawDialog(){
        hits.clear();
        if(dialog==Dialog::Options){options();return;}if(dialog==Dialog::Selection){selection();return;}if(dialog==Dialog::Score){scoreDialog();return;}
        if(dialog==Dialog::About){messageBox({"Scopa Free","Linux edition by Music Monster, 2026","","Based on Scopa Free for Windows 1.06","by David Bernazzani (1999)","","Italian card faces by Michael P. Reed."},"OK",[this]{dialog=Dialog::None;},350);return;}
        if(dialog==Dialog::Statistics){messageBox({"Scopa games played = "+std::to_string(gamesPlayed),"Scopa games won = "+std::to_string(gamesWon),"Win Percentage = "+std::to_string(gamesPlayed?100*gamesWon/gamesPlayed:0)+"%","","Statistics are for this session."},"OK",[this]{dialog=Dialog::None;});return;}
        if(dialog==Dialog::Rules){messageBox({"Scopa Free Help","","Click the stock to deal. Click a card in your hand to select it.","Then click a matching table card to capture, or an empty table","space to place it. For a sum capture, click each table card.","A matching single card takes priority unless Allow All Captures is on.","Click the stock again when both hands have been played.","","Most cards, most coins, seven of coins and best primiera score 1 each.","Tied categories score neither player. Primiera needs all four suits:","7=21, 6=18, ace=16, 5=15, 4=14, 3=13, 2=12, face cards=10.","Score 1 for Sweeps enables scopa points; never on the final play.","Scopa D'Assi: an ace without a matching ace takes the table, no sweep.","Napoli: 3+ consecutive coins from the ace score their count.","Re Bello: the king of coins scores 1. Cappotto: all 40 cards score","the specified amount instead of normal points for that round.","","F2 New random, F3 Select game, F4 Replay, F5 Options, F6 Statistics.","F7 Game score. 1/2/3 Select card, Enter Play, Tab Capture, H Hint.","Space Deal. M Sound. B Card back. Escape Cancel or close."},"OK",[this]{dialog=Dialog::None;},470);return;}
        messageBox({error},"OK",[this]{dialog=Dialog::None;});
    }
    void mouseToClient(int x,int y,int& cx,int& cy){
        if(windowW>0&&windowH>0){cx=x*outputW/windowW;cy=y*outputH/windowH;}else{cx=x;cy=y;}
    }
    void paint(){drawBoard();SDL_RenderPresent(renderer);}
    void click(int x,int y){for(auto it=hits.rbegin();it!=hits.rend();++it)if(x>=it->r.x&&x<it->r.x+it->r.w&&y>=it->r.y&&y<it->r.y+it->r.h){auto action=it->fn;action();return;}if(menu)menu=0;}
    void key(SDL_Keycode k,Uint16 mod){
        if(k==SDLK_ESCAPE){if(dialog==Dialog::Score&&scoreContinues)return;dialog=Dialog::None;menu=0;clearSelection();SDL_StopTextInput();return;}
        if(dialog==Dialog::Selection||dialog==Dialog::Options){
            std::string* s=edit==1?&seedText:edit==2?&pointsText:edit==3?&cappottoText:nullptr;
            if(s&&k==SDLK_BACKSPACE&&!s->empty())s->pop_back();
            if(s&&k==SDLK_a&&(mod&KMOD_CTRL))s->clear();
            if(dialog==Dialog::Selection&&k==SDLK_RETURN)useSeed(0);
            return;
        }
        if(dialog!=Dialog::None){if(k==SDLK_RETURN){if(dialog==Dialog::Score)continueScore();else dialog=Dialog::None;}return;}
        if(mod&KMOD_ALT){if(k==SDLK_g){menu=1;menuItem=0;}else if(k==SDLK_h){menu=2;menuItem=0;}return;}
        if(k>=SDLK_F2&&k<=SDLK_F7){command(k-SDLK_F2);return;}if(k==SDLK_F1){menu=0;dialog=Dialog::Rules;return;}
        if(menu){if(k==SDLK_DOWN)menuItem=(menuItem+1)%(menu==1?7:2);else if(k==SDLK_UP)menuItem=(menuItem+(menu==1?6:1))%(menu==1?7:2);else if(k==SDLK_RETURN){if(menu==1)command(menuItem);else{dialog=menuItem?Dialog::About:Dialog::Rules;menu=0;}}return;}
        if(k==SDLK_SPACE){deal();return;}if(k==SDLK_m){sound=!sound;return;}if(k==SDLK_b){back=(back+1)%8;return;}
        if(game.turn||game.over||!dealt||waitingDeal)return;
        if(k>=SDLK_1&&k<=SDLK_3){int c=handSlots[0][k-SDLK_1];if(c>=0)selectCard(c);}
        else if(k==SDLK_RETURN)keyboardPlay();else if(k==SDLK_h)hint();else if(k==SDLK_TAB&&!choices.empty()){choice=(choice+1)%choices.size();chosen=choices[choice];}
    }
public:
    App(fs::path path,uint32_t seed,std::string shot,bool startDealt):assets(path),game(seed),screenshot(shot){
        if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)!=0)throw std::runtime_error(SDL_GetError());
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR,"0");
        SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS,"0");
        window=SDL_CreateWindow("Scopa Free",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,kCanvasW,kCanvasH,SDL_WINDOW_HIDDEN|SDL_WINDOW_RESIZABLE);
        if(!window)throw std::runtime_error(SDL_GetError());
        SDL_SetWindowMinimumSize(window,kCanvasW/2,kCanvasH/2);
        renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);if(!renderer)throw std::runtime_error(SDL_GetError());
        // icon.bmp is prepared as 64x64 LANCZOS BITMAPV4 (see tools/prepare_icon.py) for panel quality.
        auto icon=SDL_LoadBMP((assets/"icon.bmp").c_str());if(icon){SDL_SetWindowIcon(window,icon);SDL_FreeSurface(icon);}
        font=load("winfont.bmp",true);std::ifstream metrics(assets/"winfont-widths.bin",std::ios::binary);metrics.read(reinterpret_cast<char*>(widths.data()),256);if(!metrics)throw std::runtime_error("Cannot read bitmap font metrics");
        for(int c=0;c<40;++c){int r=rank(c),index=suit(c)*13+(r<=7?r-1:r==8?11:r==9?10:12);char file[32];snprintf(file,sizeof file,"card%02d.bmp",index);cards[c]=load(file);negative[c]=load(file,false,true);}
        for(int i=0;i<8;++i)backs[i]=load("cardback"+std::to_string(i+1)+".bmp");
        empty=load("emptyback.bmp");dashed=load("emptydashed.bmp");foundation=load("foundationgeneric.bmp");menubar=load("menu-bar.bmp");
        if(SDL_InitSubSystem(SDL_INIT_AUDIO)==0){SDL_AudioSpec want{},have{};want.freq=22050;want.format=AUDIO_U8;want.channels=1;want.samples=512;audio=SDL_OpenAudioDevice(nullptr,0,&want,&have,0);
            if(audio)for(int i=0;i<2;++i){Uint8* data=nullptr;Uint32 size=0;SDL_AudioSpec wav{};if(SDL_LoadWAV((assets/(i?"cardflip.wav":"cardsel.wav")).c_str(),&wav,&data,&size)){sounds[i].assign(data,data+size);SDL_FreeWAV(data);}}}
        clearBoard();title();if(startDealt)deal();
        paint();SDL_ShowWindow(window);
    }
    ~App(){if(audio)SDL_CloseAudioDevice(audio);for(auto t:cards)SDL_DestroyTexture(t);for(auto t:negative)SDL_DestroyTexture(t);for(auto t:backs)SDL_DestroyTexture(t);SDL_DestroyTexture(empty);SDL_DestroyTexture(dashed);SDL_DestroyTexture(foundation);SDL_DestroyTexture(menubar);SDL_DestroyTexture(font);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();}
    int run(int smoke){int frames=0;
        while(running){SDL_Event e;while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT)running=false;
            if(e.type==SDL_WINDOWEVENT&&(e.window.event==SDL_WINDOWEVENT_SIZE_CHANGED||e.window.event==SDL_WINDOWEVENT_EXPOSED))paint();
            if(e.type==SDL_MOUSEMOTION){mouseToClient(e.motion.x,e.motion.y,hoverX,hoverY);}
            if(e.type==SDL_MOUSEBUTTONDOWN&&e.button.button==SDL_BUTTON_LEFT){int x,y;mouseToClient(e.button.x,e.button.y,x,y);click(x,y);}
            if(e.type==SDL_KEYDOWN)key(e.key.keysym.sym,e.key.keysym.mod);
            if(e.type==SDL_TEXTINPUT&&(dialog==Dialog::Selection||dialog==Dialog::Options)){std::string* s=edit==1?&seedText:edit==2?&pointsText:edit==3?&cappottoText:nullptr;if(s)for(char c:std::string(e.text.text))if(c>='0'&&c<='9'&&s->size()<(edit==1?10u:3u))s->push_back(c);}
        }
        if(dialog==Dialog::None&&!menu&&dealt&&!waitingDeal&&!game.over&&game.turn==1&&SDL_TICKS_PASSED(SDL_GetTicks(),aiAt)){
            int stock=game.deck.size();game.play(1,game.bestMove(1,difficulty));if((int)game.deck.size()<stock)waitingDeal=true;sync();playSound(1);roundEnd();}
        drawBoard();
        if(++frames==2&&!screenshot.empty()){auto s=SDL_CreateRGBSurfaceWithFormat(0,kCanvasW,kCanvasH,32,SDL_PIXELFORMAT_ARGB8888);SDL_Rect src{0,0,kCanvasW,kCanvasH};SDL_RenderReadPixels(renderer,&src,s->format->format,s->pixels,s->pitch);SDL_SaveBMP(s,screenshot.c_str());SDL_FreeSurface(s);}
        SDL_RenderPresent(renderer);if(smoke&&frames>=smoke)break;SDL_Delay(16);
        }return 0;
    }
};
int main(int argc,char** argv){try{
    uint32_t seed=uint32_t(std::chrono::system_clock::now().time_since_epoch().count());int smoke=0;std::string shot;bool dealt=false;
    auto exe=fs::canonical("/proc/self/exe");fs::path assets=exe.parent_path()/"../share/scopa-free";if(!fs::exists(assets))assets=exe.parent_path()/"../assets";
    for(int i=1;i<argc;++i){std::string a=argv[i];
        if(a=="--version"){std::cout<<"Scopa Free Native 1.1.0 (classic interface)\n";return 0;}
        if(a=="--help"){std::cout<<"Scopa Free Native\n  --seed NUMBER  reproducible native match\n  --assets PATH  asset directory\n  --dealt        start with the initial deal visible\n  --smoke-test   open, render and close\n  --screenshot FILE.bmp  save a rendered frame\n  --version\n";return 0;}
        if(a=="--smoke-test")smoke=5;else if(a=="--dealt")dealt=true;
        else if((a=="--seed"||a=="--assets"||a=="--screenshot")&&i+1<argc){std::string value=argv[++i];if(a=="--seed"){size_t end;auto n=std::stoull(value,&end);if(end!=value.size()||n<1||n>UINT32_MAX)throw std::runtime_error("Seed must be 1..4294967295");seed=uint32_t(n);}else if(a=="--assets")assets=value;else shot=value;}
        else throw std::runtime_error("Unknown or incomplete option: "+a);
    }
    App app(assets,seed,shot,dealt);return app.run(smoke);
}catch(const std::exception& e){std::cerr<<"Scopa Free: "<<e.what()<<"\n";return 1;}}
