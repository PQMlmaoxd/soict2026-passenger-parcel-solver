#include <bits/stdc++.h>
using namespace std;
using ll=long long;
using vi=vector<int>;
using vl=vector<ll>;
template<class T>using vc=vector<T>;
using clk=chrono::steady_clock;
#define all(x) (x).begin(),(x).end()
#define sz(x) int((x).size())
const ll INF=LLONG_MAX/4,BAD=-INF;

struct FastIn {
  static constexpr size_t SZ = 1<<16;
  char buffer[SZ]; size_t pos=0, len=0;
  int get() {
    if(pos==len) { len=fread(buffer,1,SZ,stdin); pos=0; if(!len)return EOF; }
    return static_cast<unsigned char>(buffer[pos++]);
  }
  ll next() {
    int c=get();
    for(;;) {
      while(c!=EOF && c<=32)c=get();
      if(c!='#')break;
      while(c!=EOF && c!='\n')c=get();
    }
    if(c==EOF)throw runtime_error("Unexpected end of input");
    bool neg=c=='-'; if(neg)c=get();
    if(c<'0'||c>'9')throw runtime_error("Expected an integer");
    ll x=0;
    while(c>='0'&&c<='9') {
      if(x>(INF-(c-'0'))/10)throw runtime_error("Integer is too large");
      x=x*10+c-'0'; c=get();
    }
    if(c=='#')while(c!=EOF&&c!='\n')c=get();
    else if(c!=EOF&&c>32)throw runtime_error("Invalid character after integer");
    return neg?-x:x;
  }
};
struct Req { int p,d; ll w=0,e,l,ed=0,ld=INF,s,rev; };
struct Node { int u,v; ll l,r,dt,w,val,ic; };
struct Rt {
  vi a;
  vl tm,lim,w;
  ll f=0;
};
struct Sol { vc<Rt> a; vi id; ll f=0; };
struct Ins {
  int k=-1,i=0,j=0; ll gain=BAD,delay=0;
  bool ok() const { return k>=0; }
};
struct Col { vi a,ids; ll f; };
struct ALNS {
  vc<double> wgt,scr; vi cnt;
  explicit ALNS(int n):wgt(n,1),scr(n,0),cnt(n,0){}
  int pick(mt19937_64& rng) {
    return discrete_distribution<int>(all(wgt))(rng);
  }
  void val(int x,double v) { scr[x]+=v; cnt[x]++; }
  void update() {
    for(size_t i=0;i<wgt.size();i++) {
      if(cnt[i])wgt[i]=max(0.1,0.8*wgt[i]+0.2*scr[i]/cnt[i]);
      scr[i]=0; cnt[i]=0;
    }
  }
};

int K=0,N=0,M=0,R=0,V=0;
vi O;
vl Q,tmx,cst;
vc<Req> rq;
vc<Node> ev;
vc<vc<Col>> pool;
vc<double> ph;
Sol ans;
mt19937_64 rng;
clk::time_point st;
double TL=294.9,gs=1,gt=1;
uint64_t iter=0;
bool ex=true;

static bool detClock=false; static uint64_t detTicks=0;
double getTime() { if(detClock)return double(++detTicks)*1e-9; return chrono::duration<double>(clk::now()-st).count(); }
bool out(double until) { return getTime()>=min(until,TL); }
double rnd() { return generate_canonical<double,53>(rng); }
int rd(int n) { return n?int(rng()%uint64_t(n)):0; }
ll t(int u,int v) { return tmx[size_t(u)*V+v]; }
ll c(int u,int v) { return cst[size_t(u)*V+v]; }
double& tau(int r,int k) { return ph[size_t(r)*K+k]; }
bool isP(int r) { return r<N; }

Sol emptySol() {
  Sol s; s.a.resize(K); s.id.assign(R,-1); return s;
}

bool eval(int k,Rt& z) {
  int L=sz(z.a),u=O[k];
  ll tm=0,w=0,value=0;
  z.tm.resize(L); z.lim.resize(L); z.w.resize(L);
  for(int i=0;i<L;i++) {
    const Node& b=ev[z.a[i]];
    tm=max(b.l,tm+t(u,b.u));
    if(tm>b.r)return false;
    tm+=b.dt; w+=b.w;
    if(w<0||w>Q[k])return false;
    value+=b.val-b.ic-c(u,b.u);
    z.tm[i]=tm; z.w[i]=w; u=b.v;
  }
  if(w!=0)return false;
  for(int i=L-1;i>=0;i--) {
    z.lim[i]=ev[z.a[i]].r;
    if(i+1<L)z.lim[i]=min(z.lim[i],z.lim[i+1]
      -ev[z.a[i]].dt-t(ev[z.a[i]].v,ev[z.a[i+1]].u));
  }
  z.f=value; return true;
}
void syncSol(Sol& s) {
  s.id.assign(R,-1); s.f=0;
  for(int k=0;k<K;k++) {
    s.f+=s.a[k].f;
    for(int x:s.a[k].a)s.id[x/2]=k;
  }
}
bool check(Sol& s) {
  vi state(R,0),assigned(R,-1);
  for(int k=0;k<K;k++) {
    for(int x:s.a[k].a) {
      if(x<0||x>=2*R)return false;
      int r=x/2;
      if(isP(r)) {
        if((x&1)||state[r])return false;
        state[r]=2; assigned[r]=k;
      } else if(!(x&1)) {
        if(state[r])return false;
        state[r]=1; assigned[r]=k;
      } else {
        if(state[r]!=1||assigned[r]!=k)return false;
        state[r]=2;
      }
    }
    if(!eval(k,s.a[k]))return false;
  }
  for(int x:state)if(x==1)return false;
  syncSol(s); return true;
}
void save(int k,const Rt& z) {
  if(z.f<=0||z.a.empty())return;
  vi ids;
  for(int x:z.a)if(!(x&1))ids.push_back(x/2);
  sort(all(ids));
  auto& columns=pool[k];
  for(auto& col:columns)if(col.ids==ids) {
    if(z.f>col.f)col={z.a,move(ids),z.f};
    return;
  }
  columns.push_back({z.a,move(ids),z.f});
  int cap=max(16,min(160,12000/max(1,K)));
  if(sz(columns)>cap) {
    
    auto bucket=[](size_t n){return n<=2?0:n<=4?1:n<=8?2:3;};
    array<int,4> counts{};
    for(const auto& col:columns)counts[bucket(col.ids.size())]++;
    int crowded=int(max_element(all(counts))-counts.begin());
    bool diversity=rnd()<0.75; int worst=-1;
    for(int i=0;i<sz(columns);i++) {
      if(diversity&&bucket(columns[i].ids.size())!=crowded)continue;
      if(worst<0||columns[i].f<columns[worst].f)worst=i;
    }
    columns.erase(columns.begin()+worst);
  }
}
void upd(const Sol& s) {
  ll cleaned=s.f;
  for(const auto& route:s.a)if(route.f<0)cleaned-=route.f;
  if(cleaned>ans.f) {
    ans=s;

    for(auto& route:ans.a)if(route.f<=0)route=Rt();
    syncSol(ans);
    for(int k=0;k<K;k++)save(k,ans.a[k]);
  }
}
vi makeRoute(const vi& a,int r,const Ins& p) {
  vi b; b.reserve(a.size()+2);
  for(int i=0;i<=sz(a);i++) {
    if(i==p.i)b.push_back(2*r);
    if(!isP(r)&&i==p.j)b.push_back(2*r+1);
    if(i<sz(a))b.push_back(a[i]);
  }
  return b;
}
void put(Sol& s,int r,const Ins& p) {
  Rt z; z.a=makeRoute(s.a[p.k].a,r,p);
  if(!eval(p.k,z))throw logic_error("Invalid insertion");
  if(z.f-s.a[p.k].f!=p.gain)throw logic_error("Stale insertion gain");
  s.f+=z.f-s.a[p.k].f;
  s.a[p.k]=move(z); s.id[r]=p.k;
}

Ins findMove(const Sol& s,int r,int k,double until) {
  Ins bestMove;
  const Rt& z=s.a[k]; const Req& q=rq[r];
  if(q.w>Q[k])return bestMove;
  int L=sz(z.a);
  for(int i=0;i<=L;i++) {
    if((i&15)==0&&out(until))break;
    int prev=i?ev[z.a[i-1]].v:O[k];
    ll before=i?z.tm[i-1]:0;
    ll begin=max(q.e,before+t(prev,q.p));
    if(begin>q.l)continue;
    ll oldEdge=i<L?c(prev,ev[z.a[i]].u):0;
    if(isP(r)) {
      ll finish=begin+ev[2*r].dt;
      if(i<L&&finish+t(q.d,ev[z.a[i]].u)>z.lim[i])continue;
      ll delta=c(prev,q.p)+c(q.p,q.d)-oldEdge;
      if(i<L)delta+=c(q.d,ev[z.a[i]].u);
      ll gain=q.rev-delta;
      ll delay=i<L?max<ll>(0,finish+t(q.d,ev[z.a[i]].u)
        -(z.tm[i]-ev[z.a[i]].dt)):finish-before;
      if(gain>bestMove.gain)bestMove={k,i,i,gain,delay};
      continue;
    }
    ll baseLoad=i?z.w[i-1]:0;
    if(baseLoad+q.w>Q[k])continue;
    ll tm=begin+q.s; int u=q.p;
    ll pickupDelta=c(prev,q.p)-oldEdge;
    if(i<L)pickupDelta+=c(q.p,ev[z.a[i]].u);
    for(int j=i;j<=L;j++) {
      if((j&63)==0&&out(until))break;
      ll delivery=max(q.ed,tm+t(u,q.d));
      if(delivery<=q.ld && (j==L||delivery+q.s+
        t(q.d,ev[z.a[j]].u)<=z.lim[j])) {
        ll delta;
        if(j==i) {
          delta=c(prev,q.p)+c(q.p,q.d)-oldEdge;
          if(j<L)delta+=c(q.d,ev[z.a[j]].u);
        } else {
          delta=pickupDelta+c(u,q.d);
          if(j<L)delta+=c(q.d,ev[z.a[j]].u)-c(u,ev[z.a[j]].u);
        }
        ll gain=q.rev-delta;
        ll delay=j<L?max<ll>(0,delivery+q.s+t(q.d,ev[z.a[j]].u)
          -(z.tm[j]-ev[z.a[j]].dt)):delivery+q.s-(L?z.tm.back():0);
        if(gain>bestMove.gain)bestMove={k,i,j,gain,max<ll>(0,delay)};
      }
      if(j==L)break;
      const Node& b=ev[z.a[j]];
      tm=max(b.l,tm+t(u,b.u));
      if(tm>b.r||z.w[j]+q.w>Q[k])break;
      tm+=b.dt; u=b.v;
    }
  }
  return bestMove;
}
vi taxiList(const Sol& s,int r,int forced=-1) {
  vi taxis;
  if(K<=24) { taxis.resize(K); iota(all(taxis),0); return taxis; }
  vc<pair<double,int>> rank,early;
  for(int k=0;k<K;k++)if(rq[r].w<=Q[k]) {
    ll near=c(O[k],rq[r].p),arrival=t(O[k],rq[r].p);
    const auto& a=s.a[k].a;
    for(int i=0;i<sz(a);i++){
      int u=ev[a[i]].v;near=min(near,c(u,rq[r].p));
      arrival=min(arrival,s.a[k].tm[i]+t(u,rq[r].p));
    }
    if(max(arrival,rq[r].e)>rq[r].l)continue;
    double scr=double(near)-0.1*gs*tau(r,k)+rnd()*0.02*gs;
    rank.emplace_back(scr,k);early.emplace_back(double(arrival),k);
  }
  int cnt=min((R<=120||(iter>0&&iter%19==0))?K:16,sz(rank));
  sort(all(rank));
  for(int i=0;i<cnt;i++)taxis.push_back(rank[i].second);
  sort(all(early));
  for(int i=0;i<min(6,sz(early));i++)taxis.push_back(early[i].second);
  for(int j=0;j<4;j++)taxis.push_back(rd(K));
  if(forced>=0)taxis.insert(taxis.begin(),forced);
  vc<char> seen(K,0);vi unique;
  for(int k:taxis)if(!seen[k]){seen[k]=1;unique.push_back(k);}
  taxis=move(unique);
  return taxis;
}
Ins bestMove(const Sol& s,int r,double until,int force=-1) {
  Ins p;
  for(int k:taxiList(s,r,force)) {
    if(out(until))break;
    auto v=findMove(s,r,k,until); if(v.gain>p.gain)p=v;
  }
  return p;
}
vi used(const Sol& s) {
  vi ids; for(int r=0;r<R;r++)if(s.id[r]>=0)ids.push_back(r); return ids;
}

void eraseReq(Sol& s,const vi& ids) {
  vc<char> cut(R,0),changed(K,0);
  for(int r:ids)if(s.id[r]>=0) { cut[r]=1; changed[s.id[r]]=1; }
  for(int k=0;k<K;k++)if(changed[k]) {
    Rt z;
    for(int x:s.a[k].a)if(!cut[x/2])z.a.push_back(x);
    while(!eval(k,z)){
      int u=O[k],bad=-1;ll tm=0,load=0;
      for(int x:z.a){
        const auto& b=ev[x];tm=max(b.l,tm+t(u,b.u));load+=b.w;
        if(tm>b.r||load<0||load>Q[k]){bad=x/2;break;}
        tm+=b.dt;u=b.v;
      }
      if(bad<0){z=Rt();eval(k,z);break;}
      z.a.erase(remove_if(all(z.a),[&](int x){return x/2==bad;}),z.a.end());
    }
    s.a[k]=move(z);
  }
  syncSol(s);
}
vi reqList(const Sol& s,const vi& priority,int cap) {
  vi ids; vc<char> seen(R,0);
  for(int r:priority)if(s.id[r]<0&&!seen[r]) { ids.push_back(r); seen[r]=1; }
  vc<pair<double,int>> others;
  for(int r=0;r<R;r++)if(s.id[r]<0&&!seen[r])
    others.emplace_back(-(double(rq[r].rev)+gs*rnd()*2),r);
  int take=max(0,cap-sz(ids)); take=min(take,sz(others));

  int ranked=take==sz(others)?take:(2*take/3);
  partial_sort(others.begin(),others.begin()+ranked,others.end());
  for(int j=0;j<ranked;j++)ids.push_back(others[j].second);
  shuffle(others.begin()+ranked,others.end(),rng);
  for(int j=ranked;j<take;j++)ids.push_back(others[j].second);
  shuffle(all(ids),rng);
  return ids;
}

void repair(Sol& s,int mode,const vi& priority,double until,bool full=false,
      const vi* oldOwner=nullptr) {
  int cap=full?R:(R<=120?R:96);
  auto ids=reqList(s,priority,cap);
  struct Cache { vi taxis,version; vc<Ins> moves; };
  vc<Cache> cache(ids.size()); vi version(K,0);
  for(size_t z=0;z<ids.size();z++) {
    cache[z].taxis=taxiList(s,ids[z],oldOwner?(*oldOwner)[ids[z]]:-1);
    cache[z].version.assign(cache[z].taxis.size(),-1);
    cache[z].moves.resize(cache[z].taxis.size());
  }
  int explorations=0;
  while(!out(until)) {
    double potential=gs;
    if(mode==5)for(int r:ids)if(s.id[r]<0)potential+=double(rq[r].rev);
    int chosen=-1; Ins chosenMove; double bestScore=-1e300;
    for(size_t z=0;z<ids.size()&&!out(until);z++) {
      int r=ids[z]; if(s.id[r]>=0)continue;
      auto& cc=cache[z]; vc<Ins> options;
      for(size_t j=0;j<cc.taxis.size()&&!out(until);j++) {
        int k=cc.taxis[j];
        if(cc.version[j]!=version[k]) {
          cc.moves[j]=findMove(s,r,k,until); cc.version[j]=version[k];
        }
        if(cc.moves[j].ok())options.push_back(cc.moves[j]);
      }
      if(options.empty())continue;
      sort(all(options),[](const Ins& a,const Ins& b){return a.gain>b.gain;});
      Ins p=options[0];
      if(mode==4) {
        double mx=-1e300;
        for(auto v:options)if(v.gain>0) {
          double sc=double(v.gain)+0.15*gs*log(max(0.02,tau(r,v.k)));
          if(sc>mx) { mx=sc; p=v; }
        }
      }
      if(p.gain<=0&&(mode!=5||explorations>=(s.f<=0?8:2)||double(p.gain)<-potential))continue;
      double scr=double(p.gain);
      if(mode==1||mode==2) {
        int depth=mode==1?2:3;
        
        double regret=0;
        for(int j=1;j<depth;j++)regret+=double(p.gain-(j<sz(options)?max<ll>(0,options[j].gain):0));
        scr+=0.7*regret;
      }
      if(mode==3||mode==5)scr+=(rnd()-0.5)*gs*0.5;
      if(mode==4)scr+=0.15*gs*log(max(0.02,tau(r,p.k)));
      if(mode==6)scr/=1.0+0.45*double(p.delay)/gt;
      if(mode==7)scr-=0.20*gs*double(p.delay)/gt;
      if(scr>bestScore) { bestScore=scr; chosen=r; chosenMove=p; }
    }
    if(chosen<0||out(until))break;
    if(chosenMove.gain<=0)explorations++;
    put(s,chosen,chosenMove); version[chosenMove.k]++;
    if(K>24&&mode==5)for(auto& cc:cache)
      if(find(all(cc.taxis),chosenMove.k)==cc.taxis.end()){
        cc.taxis.push_back(chosenMove.k);cc.version.push_back(-1);cc.moves.push_back(Ins());
      }
    upd(s);
  }
}
vi ruin(Sol& s,int mode,int amount,double until) {
  auto ids=used(s); if(ids.empty())return {};
  int anchor=ids[rd(sz(ids))];
  vc<pair<double,int>> rank;
  if(mode==0)shuffle(all(ids),rng);
  else if(mode==1||mode==4) {
    for(int r:ids) {
      double d=double(t(rq[anchor].p,rq[r].p))+double(t(rq[anchor].d,rq[r].d))
        +0.3*abs(double(rq[anchor].e)-double(rq[r].e));
      if(s.id[r]!=s.id[anchor])d+=0.3*gs;
      if(mode==4&&isP(r))d+=gs;
      rank.emplace_back(d*(0.8+0.4*rnd()),r);
    }
  } else if(mode==2) {
    for(int r:ids) {
      if(out(until))break;
      int k=s.id[r]; Rt z;
      for(int x:s.a[k].a)if(x/2!=r)z.a.push_back(x);
      double benefit=eval(k,z)?double(z.f-s.a[k].f):-gs*10;
      rank.emplace_back(-benefit+rnd()*gs*0.1,r);
    }
  } else if(mode==3) {
    int k=s.id[anchor];
    for(int r:ids)rank.emplace_back((s.id[r]==k?0.0:1.0)+rnd(),r);
    amount=max(amount,int(count_if(all(ids),[&](int r){return s.id[r]==k;})));
  } else if(mode==5) {
    const auto& a=s.a[s.id[anchor]].a;
    int at=rd(sz(a));
    vi position(R,-1000000);
    for(int i=0;i<sz(a);i++)position[a[i]/2]=i;
    for(int r:ids)rank.emplace_back(abs(position[r]-at)+rnd(),r);
  } else {
    for(int r:ids)rank.emplace_back(abs(double(rq[r].l)-double(rq[anchor].l))+rnd()*gs,r);
  }
  if(!rank.empty()) {
    sort(all(rank)); ids.clear();
    for(auto x:rank)ids.push_back(x.second);
  }
  ids.resize(min(amount,sz(ids)));
  auto before=s.id; eraseReq(s,ids);
  
  ids.clear();
  for(int r=0;r<R;r++)if(before[r]>=0&&s.id[r]<0)ids.push_back(r);
  return ids;
}

bool optRoute(Sol& s,int k,double until) {
  bool improved=false;
  for(int pass=0;pass<2&&!out(until);pass++) {
    const auto& z=s.a[k]; const auto& a=z.a;
    int L=sz(a); if(L<2)return improved;
    vi position(2*R,-1),order(L);
    for(int i=0;i<L;i++)position[a[i]]=i;
    iota(all(order),0); shuffle(all(order),rng);
    ll gain=0; Rt winner;
    for(int i:order) {
      if(out(until))break;
      int x=a[i],u=i?ev[a[i-1]].v:O[k];
      ll saving=c(u,ev[x].u);
      if(i+1<L)saving+=c(ev[x].v,ev[a[i+1]].u)-c(u,ev[a[i+1]].u);
      int offset=rd(L+1);
      for(int h=0;h<=L;h++) {
        int j=(h+offset)%(L+1);
        if((h&31)==0&&out(until))break;
        if(j==i||j==i+1)continue;
        if(!isP(x/2)) {
          if(!(x&1)&&j>position[x+1])continue;
          if((x&1)&&j<=position[x-1])continue;
        }
        int prev=j?ev[a[j-1]].v:O[k];
        ll extra=c(prev,ev[x].u);
        if(j<L)extra+=c(ev[x].v,ev[a[j]].u)-c(prev,ev[a[j]].u);
        if(saving-extra<=gain)continue;
        Rt candidate; candidate.a=a;
        candidate.a.erase(candidate.a.begin()+i);
        candidate.a.insert(candidate.a.begin()+j-(i<j?1:0),x);
        if(eval(k,candidate)) { gain=candidate.f-z.f; winner=move(candidate); }
      }
    }
    vl reverseDelta(L,0);
    for(int i=1;i<L;i++)reverseDelta[i]=reverseDelta[i-1]
      +c(ev[a[i]].v,ev[a[i-1]].u)-c(ev[a[i-1]].v,ev[a[i]].u);
    for(int i:order) {
      if(out(until))break;
      int prev=i?ev[a[i-1]].v:O[k];
      for(int j=i+1;j<L;j++) {
        if((j&31)==0&&out(until))break;
        ll saving=c(prev,ev[a[i]].u)-c(prev,ev[a[j]].u)
          -(reverseDelta[j]-reverseDelta[i]);
        if(j+1<L)saving+=c(ev[a[j]].v,ev[a[j+1]].u)-c(ev[a[i]].v,ev[a[j+1]].u);
        if(saving<=gain)continue;
        bool ok=true;
        for(int h=i;h<=j;h++)if(!isP(a[h]/2)&&!(a[h]&1)&&position[a[h]+1]<=j) {
          ok=false; break;
        }
        if(!ok)continue;
        Rt candidate; candidate.a=a;
        reverse(candidate.a.begin()+i,candidate.a.begin()+j+1);
        if(eval(k,candidate)) { gain=candidate.f-z.f; winner=move(candidate); }
      }
    }
    if(gain<=0)break;
    s.f+=gain; s.a[k]=move(winner); improved=true;
    save(k,s.a[k]); upd(s);
  }
  return improved;
}
void crossTail(Sol& s,double until) {
  if(K<2)return;
  for(int rep=0;rep<min(24,2*K)&&!out(until);rep++) {
    int a=rd(K),b=rd(K); if(a==b)continue;
    const auto& x=s.a[a].a; const auto& y=s.a[b].a;
    auto cuts=[&](const vi& route) {
      vi points={0}; int open=0;
      for(int i=0;i<sz(route);i++) {
        int event=route[i];
        if(!isP(event/2))open+=(event&1)?-1:1;
        if(!open)points.push_back(i+1);
      }
      shuffle(all(points),rng); return points;
    };
    auto first=cuts(x),second=cuts(y);
    ll gain=0; Rt winA,winB;
    for(int i:first) {
      if(out(until))break;
      int u=i?ev[x[i-1]].v:O[a];
      for(int j:second) {
        if(out(until))break;
        int v=j?ev[y[j-1]].v:O[b];
        ll delta=0;
        if(i<sz(x))delta+=c(u,ev[x[i]].u)-c(v,ev[x[i]].u);
        if(j<sz(y))delta+=c(v,ev[y[j]].u)-c(u,ev[y[j]].u);
        if(delta<=gain)continue;
        Rt na,nb;
        na.a.insert(na.a.end(),x.begin(),x.begin()+i); na.a.insert(na.a.end(),y.begin()+j,y.end());
        nb.a.insert(nb.a.end(),y.begin(),y.begin()+j); nb.a.insert(nb.a.end(),x.begin()+i,x.end());
        if(eval(a,na)&&eval(b,nb)) { gain=delta; winA=move(na); winB=move(nb); }
      }
    }
    if(gain>0) {
      s.a[a]=move(winA); s.a[b]=move(winB); syncSol(s);
      save(a,s.a[a]); save(b,s.a[b]); upd(s);
    }
  }
}

bool optBlock(Sol& s,int k,int begin,int length,double until) {
  if(k<0||k>=K||begin<0||out(until))return false;
  const Rt& original=s.a[k];
  int routeLength=sz(original.a);
  if(begin>=routeLength)return false;
  length=min({length,11,routeLength-begin});
  if(length<2)return false;
  int end=begin+length;
  vi events(original.a.begin()+begin,original.a.begin()+end);
  int u0=begin?ev[original.a[begin-1]].v:O[k];
  ll t0=begin?original.tm[begin-1]:0;
  ll w0=begin?original.w[begin-1]:0;
  int nxt=end<routeLength?original.a[end]:-1;

  ll oldCost=0;
  int previous=u0;
  for(int x:events) {
    oldCost+=c(previous,ev[x].u);
    previous=ev[x].v;
  }
  if(nxt>=0)oldCost+=c(previous,ev[nxt].u);
  if(oldCost<=0)return false;

  int states=1<<length,full=states-1;
  vi pre(length,0);
  for(int j=0;j<length;j++)if(events[j]&1) {

    for(int i=0;i<length;i++)
      if(events[i]==(events[j]^1))pre[j]|=1<<i;
  }
  vl ld(states,w0);
  for(int mask=1;mask<states;mask++) {
    int bit=mask&-mask,index=0;
    while((1<<index)!=bit)index++;
    ld[mask]=ld[mask^bit]+ev[events[index]].w;
  }

  struct Label { ll time,cst; int parent,event; };
  constexpr size_t MAXLAB=100000;
  vc<Label> labels;
  labels.reserve(MAXLAB);
  labels.push_back({t0,0,-1,-1});
  vc<vi> fronts(size_t(states)*length);
  ll bestCost=oldCost;
  int winning=-1;
  bool stopped=false;
  uint64_t operations=0;

  auto extend=[&](int mask,int last,int labelId,int next) {
    if(stopped)return;
    if(((++operations)&255)==0&&out(until)) {
      stopped=true; return;
    }
    int bit=1<<next;
    if((mask&bit)||(mask&pre[next])!=pre[next])return;
    int nextMask=mask|bit;
    if(ld[nextMask]<0||ld[nextMask]>Q[k])return;
    
    Label parent=labels[labelId];
    int u=last<0?u0:ev[events[last]].v;
    const Node& event=ev[events[next]];
    ll arrival=max(event.l,parent.time+t(u,event.u));
    if(arrival>event.r)return;
    ll finish=arrival+event.dt;
    ll value=parent.cst+c(u,event.u);

    if(value>=bestCost)return;
    ll totCost=value;
    if(nextMask==full&&nxt>=0) {
      const Node& suffix=ev[nxt];
      if(finish+t(event.v,suffix.u)>original.lim[end])return;
      totCost+=c(event.v,suffix.u);
    }
    if(nextMask==full&&totCost>=bestCost)return;
    auto& front=fronts[size_t(nextMask)*length+next];
    for(int id:front) {
      const auto& old=labels[id];
      if(old.time<=finish&&old.cst<=value)return;
    }
    if(labels.size()>=MAXLAB) { stopped=true; return; }
    front.erase(remove_if(all(front),[&](int id) {
      const auto& old=labels[id];
      return finish<=old.time&&value<=old.cst;
    }),front.end());
    int newId=sz(labels);
    labels.push_back({finish,value,labelId,next});
    front.push_back(newId);
    if(nextMask==full) { bestCost=totCost; winning=newId; }
  };

  for(int next=0;next<length&&!stopped;next++)extend(0,-1,0,next);
  for(int mask=1;mask<full&&!stopped;mask++) {
    if((mask&31)==0&&out(until)) { stopped=true; break; }
    for(int last=0;last<length&&!stopped;last++)if(mask&(1<<last)) {

      const auto& front=fronts[size_t(mask)*length+last];
      for(int id:front) {
        for(int next=0;next<length&&!stopped;next++)
          if(!(mask&(1<<next)))extend(mask,last,id,next);
        if(stopped)break;
      }
    }
  }
  if(winning<0)return false;
  vi order;
  for(int id=winning;labels[id].parent>=0;id=labels[id].parent)
    order.push_back(events[labels[id].event]);
  if(sz(order)!=length)return false;
  reverse(all(order));
  Rt candidate=original;
  copy(all(order),candidate.a.begin()+begin);
  if(!eval(k,candidate)||candidate.f<=original.f)return false;
  s.f+=candidate.f-original.f;
  s.a[k]=move(candidate);
  
  save(k,s.a[k]);
  upd(s);
  return true;
}

void ls(Sol& s,double until) {
  double t0=getTime(),span=max(0.0,until-t0);
  double te=t0+0.30*span;
  for(int rep=0;rep<min(K,3)&&!out(te);rep++)optRoute(s,rd(K),te);
  double tb=t0+0.50*span;
  for(int rep=0;rep<2&&!out(tb);rep++) {
    int k=rd(K),length=sz(s.a[k].a);
    int width=min(length,tb-getTime()<0.003?8:11);
    if(width<2)continue;
    optBlock(s,k,rd(length-width+1),width,tb);
  }
  double te2=t0+0.85*span;
  vi ids(R); iota(all(ids),0); shuffle(all(ids),rng);

  stable_partition(all(ids),[&](int r){return s.id[r]>=0;});
  int budget=min(int(used(s).size()),32);
  for(int z=0;z<budget&&!out(te2);z++) {
    int r=ids[z],old=s.id[r]; Sol b=s;
    if(old>=0)eraseReq(b,{r});
    Sol candidate=b;
    auto p=bestMove(b,r,te2,old);
    if(p.ok()) { put(b,r,p); if(b.f>candidate.f)candidate=move(b); }
    if(candidate.f>s.f) { s=move(candidate); upd(s); }
  }
  
  for(int rep=0;rep<12&&!out(te2);rep++) {
    auto active=used(s); if(active.empty()||R<2)break;
    int a=active[rd(sz(active))], b=rd(R);
    if(rep%2==0) {
      vi outside;
      for(int r=0;r<R;r++)if(s.id[r]<0)outside.push_back(r);
      if(!outside.empty()) {
        b=outside[rd(sz(outside))];
        int candidate=outside[rd(sz(outside))];
        if(rq[candidate].rev>rq[b].rev)b=candidate;
        if(rnd()<0.6) {
          double nearest=1e300;
          for(int sample=0;sample<min(12,sz(active));sample++) {
            int id=active[rd(sz(active))];
            if(rq[b].w>Q[s.id[id]])continue;
            double related=double(t(rq[id].p,rq[b].p))+double(t(rq[id].d,rq[b].d))
              +0.25*abs(double(rq[id].e)-double(rq[b].e));
            if(related<nearest) { nearest=related; a=id; }
          }
        }
      }
    }
    if(a==b)continue;
    Sol base=s; eraseReq(base,{a,b});
    if(base.f>s.f) { s=base; upd(s); }
    for(int order=0;order<2&&!out(te2);order++) {
      Sol trial=base; int x=order?a:b,y=order?b:a;
      auto p=bestMove(trial,x,te2,s.id[x]);
      if(!p.ok())continue;
      put(trial,x,p);
      if(trial.f>s.f) { s=trial; upd(s); }
      auto q=bestMove(trial,y,te2,s.id[y]);
      if(q.ok()&&q.gain>0)put(trial,y,q);
      if(trial.f>s.f) { s=move(trial); upd(s); }
    }
  }
  crossTail(s,until);
}
void learn(const Sol& elite) {
  constexpr double rho=0.12;
  for(int r=0;r<R;r++)for(int k=0;k<K;k++)
    tau(r,k)=clamp((1-rho)*tau(r,k)+rho*(elite.id[r]==k?1.0:0.0),0.02,0.98);
}

Sol pack(double until) {
  Sol answer=ans,base=ans;
  vi taxis(K); iota(all(taxis),0);
  if(K>12) { shuffle(all(taxis),rng); taxis.resize(12); }
  double enrichEnd=getTime()+max(0.0,until-getTime())*0.12;
  for(int k:taxis) {
    vi ids;
    for(int x:ans.a[k].a)if(!(x&1))ids.push_back(x/2);
    shuffle(all(ids),rng);
    for(int j=0;j<min(12,sz(ids))&&!out(enrichEnd);j++) {
      Rt smaller;
      for(int x:ans.a[k].a)if(x/2!=ids[j])smaller.a.push_back(x);
      if(eval(k,smaller))save(k,smaller);
    }
  }
  for(int k:taxis) { save(k,ans.a[k]); base.a[k]=Rt(); }
  syncSol(base);
  vc<char> used(R,0); for(int r=0;r<R;r++)used[r]=base.id[r]>=0;
  vc<double> price(R,0);
  for(int k=0;k<K;k++) {
    int cnt=0;
    for(int x:ans.a[k].a)cnt+=!(x&1);
    double share=0.8*double(ans.a[k].f)/max(1,cnt);
    for(int x:ans.a[k].a)price[x/2]=share;
  }
  struct Choice { int k; vc<Col> cols; };
  vc<Choice> choices;
  for(int k:taxis) {
    Choice choice; choice.k=k;
    for(const auto& col:pool[k]) {
      bool ok=true; for(int r:col.ids)if(used[r]) { ok=false; break; }
      if(ok)choice.cols.push_back(col);
    }
    sort(all(choice.cols),[](const Col& a,const Col& b){return a.f>b.f;});
    if(choice.cols.size()>48) {
      vc<Col> diverse;
      vc<char> selected(choice.cols.size(),0);
      for(int j=0;j<24;j++) { selected[j]=1; diverse.push_back(choice.cols[j]); }
      vc<pair<double,int>> ranked;
      for(int j=24;j<sz(choice.cols);j++) {
        const auto& col=choice.cols[j];
        ranked.emplace_back(-double(col.f)/double(col.ids.size()),j);
      }
      partial_sort(ranked.begin(),ranked.begin()+8,ranked.end());
      for(int j=0;j<8;j++) { int id=ranked[j].second; selected[id]=1; diverse.push_back(choice.cols[id]); }
      ranked.clear();
      for(int j=24;j<sz(choice.cols);j++)if(!selected[j]) {
        double value=double(choice.cols[j].f);
        for(int r:choice.cols[j].ids)if(ans.id[r]>=0&&ans.id[r]!=k)value-=price[r];
        ranked.emplace_back(-value,j);
      }
      partial_sort(ranked.begin(),ranked.begin()+16,ranked.end());
      for(int j=0;j<16;j++)diverse.push_back(choice.cols[ranked[j].second]);
      choice.cols=move(diverse);
      sort(all(choice.cols),[](const Col& a,const Col& b){return a.f>b.f;});
    }
    
    const auto& incumbent=ans.a[k];
    if(incumbent.f>0) {
      Col col; col.a=incumbent.a; col.f=incumbent.f;
      for(int x:col.a)if(!(x&1))col.ids.push_back(x/2);
      choice.cols.push_back(move(col));
      sort(all(choice.cols),[](const Col& a,const Col& b){return a.f>b.f;});
    }
    choices.push_back(move(choice));
  }
  sort(all(choices),[](const Choice& a,const Choice& b){return a.cols.size()<b.cols.size();});
  int n=sz(choices); vl upper(n+1,0);
  for(int i=n-1;i>=0;i--)upper[i]=upper[i+1]+(choices[i].cols.empty()?0:choices[i].cols[0].f);
  vi selected(n,-1),winning; ll record=answer.f; uint64_t nodes=0;
  function<void(int,ll)> dfs=[&](int d,ll value) {
    if(((++nodes&255)==0&&out(until))||value+upper[d]<=record)return;
    if(d==n) { record=value; winning=selected; return; }
    if(out(until))return;
    for(int j=0;j<sz(choices[d].cols);j++) {
      const auto& col=choices[d].cols[j];
      bool ok=true; for(int r:col.ids)if(used[r]) { ok=false; break; }
      if(!ok)continue;
      for(int r:col.ids)used[r]=1;
      selected[d]=j; dfs(d+1,value+col.f);
      for(int r:col.ids)used[r]=0;
      if(out(until))break;
    }
    selected[d]=-1; dfs(d+1,value);
  };
  dfs(0,base.f);
  if(!winning.empty()) {
    for(int d=0;d<n;d++)if(winning[d]>=0) {
      int k=choices[d].k;
      base.a[k].a=choices[d].cols[winning[d]].a;
      if(!eval(k,base.a[k]))throw logic_error("Invalid archived route");
    }
    syncSol(base); answer=move(base); upd(answer);
  }
  return answer;
}

// Fleet-wide recombination of already feasible taxi routes.  Request
// conflicts are resolved before a route can enter the working assignment.
using FleetMask=array<uint64_t,8>;
struct FleetRoute { FleetMask mask{}; const vi* path=nullptr; ll value=0; };
bool fleetOverlap(const FleetMask& a,const FleetMask& b) {
  for(size_t w=0;w<a.size();w++)if(a[w]&b[w])return true;
  return false;
}
void fleetAdd(FleetMask& a,const FleetMask& b) {
  for(size_t w=0;w<a.size();w++)a[w]|=b[w];
}

Sol globalPack(double until) {
  if(R<=120||R>512||K<8||out(until))return ans;
  Sol result=ans;
  vc<vc<FleetRoute>> routes(K);
  vi replaceable;
  for(int k=0;k<K;k++){
    if(out(until))return result;
    auto& row=routes[k];
    row.reserve(pool[k].size()+2);
    row.push_back({});
    row.push_back({});
    row[1].path=&ans.a[k].a;
    row[1].value=ans.a[k].f;
    for(int x:ans.a[k].a)if(!(x&1)){
      int r=x/2;row[1].mask[r>>6]|=uint64_t(1)<<(r&63);
    }
    for(const auto& col:pool[k]){
      if(col.a==ans.a[k].a)continue;
      FleetRoute z;z.path=&col.a;z.value=col.f;
      for(int r:col.ids)z.mask[r>>6]|=uint64_t(1)<<(r&63);
      row.push_back(z);
    }
    sort(row.begin()+2,row.end(),[](const FleetRoute& a,const FleetRoute& b){return a.value>b.value;});
    if(sz(row)>2)replaceable.push_back(k);
  }
  if(replaceable.empty())return result;
  vi current(K,1),best=current,trial(K),pending;
  pending.reserve(K);
  ll score=ans.f,record=score;
  const double start=getTime();
  const double cooling=max(0.001,until-start);
  for(int it=0;!out(until);it++){
    if(it%128==0){current=best;score=record;}
    int a=replaceable[rd(sz(replaceable))];
    auto& row=routes[a];
    int cap=min(sz(row)-2,it%5?32:sz(row)-2);
    int pick=2+rd(cap);
    if(pick==current[a])continue;
    trial=current;trial[a]=pick;
    FleetMask used=row[pick].mask;
    pending.clear();
    ll value=row[pick].value;
    for(int k=0;k<K;k++)if(k!=a){
      const auto& z=routes[k][trial[k]];
      if(!trial[k]||fleetOverlap(row[pick].mask,z.mask)||rd(100)<5) {
        trial[k]=0;pending.push_back(k);
      } else {
        fleetAdd(used,z.mask);value+=z.value;
      }
    }
    shuffle(all(pending),rng);
    for(int k:pending){
      const auto& options=routes[k];
      int chosen=0;
      if(!fleetOverlap(used,options[1].mask))chosen=1;
      for(int i=2;i<sz(options);i++){
        if(options[i].value<=options[chosen].value)break;
        if(!fleetOverlap(used,options[i].mask)){chosen=i;break;}
      }
      trial[k]=chosen;
      fleetAdd(used,options[chosen].mask);
      value+=options[chosen].value;
    }
    const double age=min(1.0,(getTime()-start)/cooling);
    const double temperature=max(0.5,0.4*gs*(1.0-age));
    if(value>=score||rnd()<exp(max(-700.0,double(value-score)/temperature))){
      current.swap(trial);score=value;
    }
    if(score>record){record=score;best=current;}
  }
  if(record<=result.f)return result;
  for(int k=0;k<K;k++){
    const auto* path=routes[k][best[k]].path;
    if(path)result.a[k].a=*path;
    else result.a[k].a.clear();
  }
  if(!check(result)||result.f!=record)throw logic_error("Invalid fleet recombination");
  upd(result);
  return result;
}

bool exactDfs(double until) {
  if(R>10||K>12||R==0||K==0)return false;
  int S=1<<R;
  // Relaxed travel times are lower bounds, including on nonmetric instances.
  vl lb=tmx;
  for(int h=0;h<V;h++)for(int u=0;u<V;u++)for(int v=0;v<V;v++)
    lb[size_t(u)*V+v]=min(lb[size_t(u)*V+v],lb[size_t(u)*V+h]+lb[size_t(h)*V+v]);
  auto dist=[&](int u,int v){return lb[size_t(u)*V+v];};
  vc<vl> value(K,vl(S,BAD));
  vc<vc<vi>> path(K,vc<vi>(S));
  map<pair<int,ll>,int> same;
  bool complete=true;
  for(int k=0;k<K&&!out(until);k++) {
    pair<int,ll> signature={O[k],Q[k]};
    if(same.count(signature)) {
      int prev=same[signature]; value[k]=value[prev]; path[k]=path[prev];
      continue;
    }
    same[signature]=k;
    unordered_map<uint64_t,vc<pair<ll,ll>>> labels;
    vi route; uint64_t nodes=0;
    function<void(int,int,int,ll,ll,ll)> dfs=[&](int picked,int done,int last,ll tm,ll w,ll f) {
      if((++nodes&255)==0&&out(until)) { complete=false; return; }
      if(!complete)return;
      int u=last<0?O[k]:ev[last].v,possible=0;
      ll optimistic=f;
      for(int r=0;r<R;r++)if(!(done>>r&1)) {
        const auto& q=rq[r];
        if(picked>>r&1) {
          if(max(q.ed,tm+dist(u,q.d))>q.ld)return;
          optimistic+=q.rev;
        } else {
          ll at=max(q.e,tm+dist(u,q.p));
          if(q.w>Q[k]||at>q.l)continue;
          if(!isP(r)&&max(q.ed,at+q.s+dist(q.p,q.d))>q.ld)continue;
          possible|=1<<r; optimistic+=q.rev;
        }
      }
      // Requests are optional; a nonpositive complete route is never necessary.
      if(optimistic<=0&&picked)return;
      uint64_t key=(uint64_t(picked)*S+done)*uint64_t(2*R+1)+(last+1);
      auto& front=labels[key];
      for(auto p:front)if(p.first<=tm&&p.second>=f)return;
      front.erase(remove_if(all(front),[&](auto p){return tm<=p.first&&f>=p.second;}),front.end());
      front.emplace_back(tm,f);
      if(picked==done&&f>value[k][done]) { value[k][done]=f; path[k][done]=route; }
      for(int r=0;r<R;r++) {
        int bit=1<<r,x;
        if(done&bit)continue;
        if(picked&bit) { if(isP(r))continue; x=2*r+1; }
        else { if(!(possible&bit))continue; x=2*r; }
        const Node& b=ev[x];
        ll at=max(b.l,tm+t(u,b.u)),nextLoad=w+b.w;
        if(at>b.r||nextLoad<0||nextLoad>Q[k])continue;
        int np=picked|bit,nd=done;
        if(isP(r)||(x&1))nd|=bit;
        route.push_back(x);
        dfs(np,nd,x,at+b.dt,nextLoad,f+b.val-b.ic-c(u,b.u));
        route.pop_back(); if(!complete)return;
      }
    };
    dfs(0,0,-1,0,0,0);
    for(int mask=1;mask<S;mask++)if(value[k][mask]>0) {
      Rt z; z.a=path[k][mask]; eval(k,z); save(k,z);
      Sol one=emptySol(); one.a[k]=move(z); syncSol(one); upd(one);
    }
    if(!complete||out(until)) { complete=false; break; }
  }
  if(out(until))complete=false;
  if(!complete)return false;
  int joined=0; vi choice(K); bool independent=true;
  for(int k=0;k<K;k++) {
    choice[k]=int(max_element(all(value[k]))-value[k].begin());
    if(joined&choice[k])independent=false;
    joined|=choice[k];
  }
  if(independent) {
    Sol optimum=emptySol();
    for(int k=0;k<K;k++) {
      optimum.a[k].a=path[k][choice[k]];
      if(!eval(k,optimum.a[k]))throw logic_error("Invalid exact route");
    }
    syncSol(optimum); upd(optimum); return true;
  }
  vc<vi> candidates(K);
  for(int k=0;k<K;k++) {
    vl sub=value[k];
    for(int bit=1;bit<S;bit<<=1)for(int mask=0;mask<S;mask++)
      if(mask&bit)sub[mask]=max(sub[mask],sub[mask^bit]);
    for(int mask=1;mask<S;mask++)if(value[k][mask]>0) {
      bool dominated=false;
      for(int bits=mask;bits;bits&=bits-1)
        if(sub[mask^(bits&-bits)]>=value[k][mask]) { dominated=true; break; }
      if(dominated)value[k][mask]=BAD;
      else candidates[k].push_back(mask);
    }
  }
  vl dp(S,BAD); dp[0]=0;
  vc<vi> parent(K,vi(S,-1));
  for(int k=0;k<K;k++) {
    vl next=dp;
    for(int mask=0;mask<S;mask++)if(dp[mask]!=BAD) {
      if((mask&63)==0&&out(until))return false;
      parent[k][mask]=0;
    }
    for(int mask=0;mask<S;mask++)if(dp[mask]!=BAD) {
      if((mask&15)==0&&out(until))return false;
      int remaining=(S-1)^mask;
      auto relax=[&](int sub) {
        int dest=mask|sub;
        if(dp[mask]+value[k][sub]>next[dest]) {
          next[dest]=dp[mask]+value[k][sub]; parent[k][dest]=sub;
        }
      };
      if(candidates[k].size()<(size_t(1)<<__builtin_popcount(unsigned(remaining)))) {
        for(int sub:candidates[k])if(!(mask&sub))relax(sub);
      } else for(int sub=remaining;sub;sub=(sub-1)&remaining)if(value[k][sub]>0)relax(sub);
    }
    dp.swap(next);
  }
  int mask=int(max_element(all(dp))-dp.begin());
  Sol optimum=emptySol();
  for(int k=K-1;k>=0;k--) {
    int sub=parent[k][mask];
    if(sub<0)throw logic_error("Broken exact DP parent");
    optimum.a[k].a=path[k][sub]; eval(k,optimum.a[k]); mask^=sub;
  }
  syncSol(optimum); upd(optimum);
  return true;
}
struct Mask {
  array<uint64_t,8> w{};
  bool operator==(const Mask& x)const{return w==x.w;}
  bool hit(Mask x)const{for(int i=0;i<8;i++)if(w[i]&x.w[i])return true;return false;}
  bool sub(Mask x)const{for(int i=0;i<8;i++)if((w[i]&x.w[i])!=w[i])return false;return true;}
  bool has(int r)const{return w[r>>6]>>(r&63)&1;}
  Mask add(int r)const{Mask x=*this;x.w[r>>6]|=1ULL<<(r&63);return x;}
  Mask operator|(Mask x)const{for(int i=0;i<8;i++)x.w[i]|=w[i];return x;}
};
struct MH {
  size_t operator()(Mask x)const{uint64_t h=0;for(auto v:x.w)h^=v+0x9e3779b97f4a7c15ULL+(h<<6)+(h>>2);return h;}
};
struct XCol {Mask mask;ll f=0,red=0,pen=0;vi a,ids;};

struct MasterResult {vi choice;ll value;bool complete;uint64_t nodes;size_t kept;};
MasterResult reducedMaster(const vc<XCol>& col,const vc<vi>& input,const vl& price,
                          const vi& incumbent,ll record,double until) {
  const ll scale=1024;
  vc<vi> rows=input;vl red(col.size()),pen(col.size());
  for(size_t i=0;i<col.size();i++){
    for(int r:col[i].ids)pen[i]+=price[r];
    red[i]=col[i].f*scale-pen[i];
  }
  for(auto&row:rows)sort(all(row),[&](int a,int b){return red[a]!=red[b]?red[a]>red[b]:col[a].f>col[b].f;});
  ll sumprice=accumulate(all(price),0LL),root=sumprice;
  for(auto&row:rows)root+=red[row.front()];
  size_t kept=0;
  for(auto&row:rows){ll top=red[row.front()];
    row.erase(remove_if(all(row),[&](int id){return root-top+red[id]<(record+1)*scale;}),row.end());
    kept+=row.size();
  }
  vi current(K),best=incumbent;bool stopped=false;uint64_t nodes=0;
  vc<char> spent(price.size(),0);vc<uint64_t> reachable(price.size(),0);uint64_t stamp=0;
  function<void(uint64_t,Mask,ll,ll)> search=[&](uint64_t assigned,Mask used,ll value,ll remaining){
    if(stopped)return;
    ++nodes;if((nodes&127)==1&&out(until)){stopped=true;return;}
    if(assigned==((1ULL<<K)-1)){
      if(value>record){record=value;best=current;}
      return;
    }
    vl tops(K,0);__int128 upper=__int128(value)*scale+remaining;
    for(int k=0;k<K;k++)if(!(assigned>>k&1)){
      bool ok=false;for(int id:rows[k])if(!used.hit(col[id].mask)){tops[k]=red[id];ok=true;break;}
      if(!ok)return;upper+=tops[k];
    }
    if(upper<(record+1)*scale)return;
    // Refund dual prices that no remaining improving choice can consume.
    // This includes request prices and the memberships of conflict cliques.
    uint64_t mark=++stamp;
    for(int k=0;k<K;k++)if(!(assigned>>k&1))for(int id:rows[k]){
      if(upper-tops[k]+red[id]<(record+1)*scale)break;
      if(!used.hit(col[id].mask))for(int p:col[id].ids)reachable[p]=mark;
    }
    for(size_t p=0;p<price.size();p++)if(!spent[p]&&reachable[p]!=mark)upper-=price[p];
    if(upper<(record+1)*scale)return;
    int taxi=-1,countbest=INT_MAX;
    for(int k=0;k<K;k++)if(!(assigned>>k&1)){
      int count=0;for(int id:rows[k]){
        if(upper-tops[k]+red[id]<(record+1)*scale)break;
        if(!used.hit(col[id].mask))++count;
      }
      if(!count)return;
      if(count<countbest){countbest=count;taxi=k;if(count==1)break;}
    }
    for(int id:rows[taxi]){
      if(upper-tops[taxi]+red[id]<(record+1)*scale)break;
      if(used.hit(col[id].mask))continue;
      current[taxi]=id;
      for(int p:col[id].ids)spent[p]=1;
      search(assigned|(1ULL<<taxi),used|col[id].mask,value+col[id].f,remaining-pen[id]);
      for(int p:col[id].ids)spent[p]=0;
      if(stopped)return;
    }
  };
  search(0,{},0,sumprice);
  return {best,record,!stopped,nodes,kept};
}

vl refineCliquePrices(const vc<XCol>& col,const vc<vi>& rows,const vl& initial,ll lower,double until){
  const ll B=1024;
  auto bound=[&](const vl& p){ll upper=accumulate(all(p),0LL);for(const auto&row:rows){ll best=0;for(int id:row){ll v=col[id].f*B;for(int r:col[id].ids)v-=p[r];best=max(best,v);}upper+=best;}return upper;};
  ll before=bound(initial);vc<double> prices(R),saved(R);for(int r=0;r<R;r++)prices[r]=saved[r]=double(initial[r])/B;
  double best=double(before)/B,alpha=1.4;int stale=0;ll total=0;for(const auto&q:rq)total+=q.rev;
  for(int it=0;it<2000&&!out(until);it++){
    vi count(R);double upper=accumulate(all(prices),0.0);
    for(const auto&row:rows){double value=0;int chosen=0;for(int id:row){double f=double(col[id].f);for(int r:col[id].ids)f-=prices[r];if(f>value){value=f;chosen=id;}}upper+=value;for(int r:col[chosen].ids)++count[r];}
    if(upper<best-1e-8){best=upper;saved=prices;stale=0;}else ++stale;
    double norm=0;for(int r=0;r<R;r++)if(prices[r]>0||count[r]>1)norm+=(count[r]-1.0)*(count[r]-1.0);
    if(norm<1e-15||upper<=double(lower)+1e-8)break;
    if(stale>=60){alpha=max(.005,alpha*.7);prices=saved;stale=0;}
    double step=alpha*max(0.0,upper-double(lower))/norm;
    for(int r=0;r<R;r++)prices[r]=min(double(total),max(0.0,prices[r]+step*(count[r]-1)));
  }
  vl proposed(R);for(int r=0;r<R;r++)proposed[r]=max(0LL,llround(saved[r]*B));
  ll after=bound(proposed);
  cerr<<"event=dual_refine before="<<before/B<<" after="<<after/B<<" elapsed="<<getTime()<<"\n";
  return after<before?proposed:initial;
}

struct CliqueResult {vi choice;ll value;bool complete=false;int rounds=0,cuts=0;uint64_t nodes=0;};
CliqueResult stableClique(const vc<XCol>& col,const vc<vi>& rows,const vl& original,
                          const vi& incumbent,ll lower,double until,bool refineAllowed=true){
  CliqueResult result;result.choice=incumbent;result.value=lower;
  const ll B=1024;vl red(col.size());ll root=accumulate(all(original),0LL);vl top(K);
  for(int k=0;k<K;k++){
    top[k]=0;
    for(int id:rows[k]){ll v=col[id].f*B;for(int r:col[id].ids)v-=original[r];red[id]=v;top[k]=max(top[k],v);}
    root+=top[k];
  }
  if(root/B<=lower){result.complete=true;return result;}
  vc<pair<int,int>> vars;
  for(int k=0;k<K;k++)for(int id:rows[k])
    if(id&&root-top[k]+red[id]>=(lower+1)*B)vars.emplace_back(k,id);
  for(int k=0;k<K;k++)if(incumbent[k]&&find(vars.begin(),vars.end(),make_pair(k,incumbent[k]))==vars.end())vars.emplace_back(k,incumbent[k]);
  int n=sz(vars);
  // Bounded memory and work on unfamiliar instances; the caller retains R's
  // incumbent and fallback when this complete-pool path is unsuitable.
  if(n>24000&&refineAllowed&&!out(until)){
    vl improved=refineCliquePrices(col,rows,original,lower,min(until,getTime()+8.0));
    return stableClique(col,rows,improved,incumbent,lower,until,false);
  }
  if(n>24000||out(until))return result;
  vc<vi> cuts;set<vi> seen;
  vc<vc<uint64_t>> edges(n,vc<uint64_t>((n+63)/64));
  auto conflict=[&](int a,int b){return (edges[a][b>>6]>>(b&63))&1ULL;};
  for(int i=0;i<n;i++){
    if((i&63)==0&&out(until))return result;
    for(int j=0;j<i;j++)if(vars[i].first==vars[j].first||col[vars[i].second].mask.hit(col[vars[j].second].mask)){
      edges[i][j>>6]|=1ULL<<(j&63);edges[j][i>>6]|=1ULL<<(i&63);
    }
  }
  vc<XCol> bestColumns;vc<vi> bestRows;vl bestPrices;vi bestInitial;ll bestBound=root;
  double lpEnd=min(until,getTime()+30.0);
  for(int round=0;round<64&&!out(lpEnd);round++){
    int m=K+R+sz(cuts),width=n+m+1;
    if(size_t(m+1)*width>12000000)break;
    vc<double> table(size_t(m+1)*width);vi basis(m);
    auto a=[&](int i,int j)->double&{return table[size_t(i)*width+j];};
    for(int i=0;i<m;i++){basis[i]=n+i;a(i,n+i)=1;a(i,width-1)=1+1e-6*double(i+1)/double(m+1);}
    for(int j=0;j<n;j++){auto[k,id]=vars[j];a(k,j)=1;for(int r:col[id].ids)a(K+r,j)=1;a(m,j)=-double(col[id].f);}
    for(int h=0;h<sz(cuts);h++)for(int j:cuts[h])a(K+R+h,j)=1;
    // Start the primal basis at the already feasible incumbent. This avoids
    // rebuilding a good fleet assignment from the all-zero degenerate vertex.
    for(int j=0;j<n;j++)if(vars[j].second==incumbent[vars[j].first]){
      int leave=vars[j].first;double pivot=a(leave,j);
      if(abs(pivot)<1e-9)continue;
      for(int v=0;v<width;v++)a(leave,v)/=pivot;a(leave,j)=1;
      for(int i=0;i<=m;i++)if(i!=leave){double f=a(i,j);if(abs(f)<1e-12){a(i,j)=0;continue;}for(int v=0;v<width;v++)a(i,v)-=f*a(leave,v);a(i,j)=0;}
      basis[leave]=j;
    }
    bool done=false;
    while(!out(lpEnd)){
      int enter=-1;double lowest=-1e-7;
      for(int j=0;j<n+m;j++)if(a(m,j)<lowest){lowest=a(m,j);enter=j;}
      if(enter<0){done=true;break;}
      int leave=-1;double ratio=1e100;
      for(int i=0;i<m;i++)if(a(i,enter)>1e-7){
        double v=a(i,width-1)/a(i,enter);
        if(v<ratio-1e-12||(abs(v-ratio)<1e-12&&(leave<0||basis[i]<basis[leave]))){ratio=v;leave=i;}
      }
      if(leave<0)break;
      double pivot=a(leave,enter);for(int j=0;j<width;j++)a(leave,j)/=pivot;a(leave,enter)=1;
      for(int i=0;i<=m;i++)if(i!=leave){double f=a(i,enter);if(abs(f)<1e-12){a(i,enter)=0;continue;}for(int j=0;j<width;j++)a(i,j)-=f*a(leave,j);a(i,enter)=0;}
      basis[leave]=enter;
    }
    vl prices(R+cuts.size());bool finite=true;
    for(int i=0;i<sz(prices);i++){
      double v=a(m,n+K+i)*B;
      if(!isfinite(v)||abs(v)>double(INF/(8*(m+1)))){finite=false;break;}
      prices[i]=max(0LL,llround(v));
    }
    if(!finite)break;
    // Expand columns per taxi, appending cut memberships only to the price
    // indices. Feasibility masks remain the actual request masks. Because each
    // cut is a conflict clique, its price can be charged at most once.
    vc<XCol> expanded(1);vc<vi> expandedRows(K,vi{0});vi initial(K,0);
    vc<unordered_map<int,int>> lookup(K);
    for(int j=0;j<n;j++){
      auto[k,id]=vars[j];int e=sz(expanded);expanded.push_back(col[id]);
      lookup[k][id]=e;expandedRows[k].push_back(e);
    }
    for(int h=0;h<sz(cuts);h++)for(int j:cuts[h])expanded[j+1].ids.push_back(R+h);
    for(int k=0;k<K;k++)if(result.choice[k]){
      auto it=lookup[k].find(result.choice[k]);
      if(it!=lookup[k].end())initial[k]=it->second;
      else{initial[k]=sz(expanded);expanded.push_back(col[result.choice[k]]);}
    }
    vl taxiPrice(K);ll bound=accumulate(all(prices),0LL);
    for(int k=0;k<K;k++){
      for(int j:expandedRows[k]){
        ll v=expanded[j].f*B;for(int r:expanded[j].ids)v-=prices[r];taxiPrice[k]=max(taxiPrice[k],v);
      }
      bound+=taxiPrice[k];
    }
    ++result.rounds;result.cuts=sz(cuts);
    vc<double> x(n);vi support;
    for(int i=0;i<m;i++)if(basis[i]<n&&a(i,width-1)>1e-7){x[basis[i]]=a(i,width-1);support.push_back(basis[i]);}
    vc<vi> supportRows(K,vi{0});
    for(int j:support)supportRows[vars[j].first].push_back(j+1);
    for(int k=0;k<K;k++)supportRows[k].push_back(initial[k]);
    for(auto&row:supportRows){sort(all(row));row.erase(unique(all(row)),row.end());}
    auto local=reducedMaster(expanded,supportRows,prices,initial,result.value,min(lpEnd,getTime()+0.15));
    result.nodes+=local.nodes;
    if(local.value>result.value){
      result.value=local.value;
      for(int k=0;k<K;k++){
        int j=local.choice[k];
        result.choice[k]=j==0?0:j<=n?vars[j-1].second:result.choice[k];
      }
    }
    if(bound/B<=result.value){result.complete=true;return result;}
    // A stronger incumbent makes global reduced-cost pruning much tighter.
    // Return it so the caller can rebuild a smaller relaxation immediately.
    if(result.value>lower)return result;
    if(bound<bestBound){
      bestBound=bound;bestColumns=move(expanded);bestRows=move(expandedRows);bestPrices=move(prices);
    }
    if(!done)break;
    sort(all(support),[&](int i,int j){return x[i]>x[j];});
    vc<pair<double,vi>> proposals;
    for(int s:support){
      if(out(lpEnd))break;
      for(int t:support)if(s<t&&conflict(s,t)){
        vi clique{s,t};double mass=x[s]+x[t];
        for(int j:support)if(j!=s&&j!=t){bool ok=true;for(int v:clique)if(!conflict(j,v)){ok=false;break;}if(ok){clique.push_back(j);mass+=x[j];}}
        sort(all(clique));if(mass>1+1e-7&&seen.insert(clique).second)proposals.emplace_back(mass-1,move(clique));
      }
    }
    if(proposals.empty())break;
    sort(all(proposals),[](const auto&a,const auto&b){if(a.first!=b.first)return a.first>b.first;return a.second<b.second;});
    for(int i=0;i<min(24,sz(proposals));i++)cuts.push_back(move(proposals[i].second));
  }
  if(!bestColumns.empty()&&!out(until)){
    vi initial(K,0);vi source(bestColumns.size(),-1);
    for(int j=0;j<n;j++)source[j+1]=vars[j].second;
    for(int k=0;k<K;k++)if(result.choice[k]){
      for(int j:bestRows[k])if(source[j]==result.choice[k]){initial[k]=j;break;}
      if(!initial[k]){initial[k]=sz(bestColumns);source.push_back(result.choice[k]);bestColumns.push_back(col[result.choice[k]]);}
    }
    auto exact=reducedMaster(bestColumns,bestRows,bestPrices,initial,result.value,until);
    result.nodes+=exact.nodes;
    if(exact.value>result.value){result.value=exact.value;for(int k=0;k<K;k++)result.choice[k]=exact.choice[k]?source[exact.choice[k]]:0;}
    result.complete=exact.complete;
  }
  return result;
}

bool exactSparse(double until) {
  if(R>512||K>63||!R||!K||out(until))return false;
  ll total=0;
  for(auto q:rq){if(q.rev>INF/1048576-total)return false;total+=q.rev;}
  double begin=getTime(),genEnd=begin+0.65*(until-begin);
  if(R>256||K>24)genEnd=min(genEnd,begin+20.0);
  vl lb=tmx;
  for(int h=0;h<V;h++){
    if(out(genEnd))return false;
    for(int u=0;u<V;u++)for(int v=0;v<V;v++)
      lb[size_t(u)*V+v]=min(lb[size_t(u)*V+v],lb[size_t(u)*V+h]+lb[size_t(h)*V+v]);
  }
  auto dist=[&](int u,int v){return lb[size_t(u)*V+v];};
  vc<XCol> col(1);vc<vi> by(K,vi(1,0));
  vi state(R,0),path;bool complete=true;
  map<pair<int,ll>,int> same;
  for(int k=0;k<K;k++){
    pair<int,ll> key={O[k],Q[k]};
    if(same.count(key)){by[k]=by[same[key]];continue;}
    same[key]=k;
    unordered_map<Mask,int,MH> index;
    ll localBest=0;uint64_t nodes=0;
    function<void(int,ll,ll,int,Mask,ll)> dfs=[&](int u,ll tm,ll w,int open,Mask mask,ll f){
      if(!complete)return;
      if((++nodes&255)==0&&(out(genEnd)||col.size()>250000||index.size()>30000)){
        complete=false;return;
      }
      if(!open&&f>0){
        auto it=index.find(mask);
        if(it==index.end()){
          int id=sz(col);index[mask]=id;
          XCol z;z.mask=mask;z.f=f;z.a=path;
          for(int x:path)if(!(x&1))z.ids.push_back(x/2);
          col.push_back(move(z));by[k].push_back(id);
        }else if(f>col[it->second].f){col[it->second].f=f;col[it->second].a=path;}
        if(f>localBest){
          localBest=f;Sol one=emptySol();one.a[k].a=path;
          if(!eval(k,one.a[k]))throw 0;
          syncSol(one);upd(one);
        }
      }
      ll optimistic=f;Mask possible;
      for(int r=0;r<R;r++)if(state[r]!=2){
        const auto& q=rq[r];
        if(state[r]==1){if(max(q.ed,tm+dist(u,q.d))>q.ld)return;optimistic+=q.rev;}
        else{
          ll at=max(q.e,tm+dist(u,q.p));
          if(q.w>Q[k]||at>q.l)continue;
          if(!isP(r)&&max(q.ed,at+q.s+dist(q.p,q.d))>q.ld)continue;
          possible=possible.add(r);optimistic+=q.rev;
        }
      }
      if(optimistic<=0)return;
      for(int r=0;r<R;r++){
        int old=state[r];if(old==2||(!old&&!possible.has(r)))continue;
        int x=2*r+(old==1);const auto& b=ev[x];
        ll at=max(b.l,tm+t(u,b.u)),nw=w+b.w;
        if(at>b.r||nw<0||nw>Q[k])continue;
        state[r]=isP(r)||old==1?2:1;path.push_back(x);
        dfs(b.v,at+b.dt,nw,open+(isP(r)?0:old==1?-1:1),mask.add(r),f+b.val-b.ic-c(u,b.u));
        path.pop_back();state[r]=old;if(!complete)return;
      }
    };
    dfs(O[k],0,0,0,{},0);
    if(out(genEnd))complete=false;
    if(!complete)break;
  }
  for(auto& row:by)sort(all(row),[&](int a,int b){return col[a].f>col[b].f;});
  vi chosen(K,0),bestChoice(K,0);ll record=0;
  auto accept=[&](const vi& a){
    Mask used;ll value=0;
    for(int id:a){if(used.hit(col[id].mask))return;used=used|col[id].mask;value+=col[id].f;}
    if(value>record){record=value;bestChoice=a;}
  };
  {
    vi base(K,0);
    for(int k=0;k<K;k++){
      Mask m;for(int x:ans.a[k].a)if(!(x&1))m=m.add(x/2);
      for(int id:by[k])if(col[id].mask==m){base[k]=id;break;}
    }
    accept(base);
  }
  auto publish=[&](){
    if(record<=ans.f)return;
    Sol s=emptySol();
    for(int k=0;k<K;k++){
      s.a[k].a=col[bestChoice[k]].a;
      if(!eval(k,s.a[k]))throw 0;
    }
    syncSol(s);upd(s);
  };
  auto archive=[&](){
    for(int k=0;k<K&&!out(until);k++)for(int j=0;j<min(12,sz(by[k]));j++){
      Rt route;route.a=col[by[k][j]].a;
      if(eval(k,route))save(k,route);
    }
  };
  vc<double> prices(R,0),saved(R,0);
  auto greedy=[&](int tries){
    vi order(K);iota(all(order),0);
    for(int it=0;it<tries&&!out(until);it++){
      shuffle(all(order),rng);Mask used;double factor=rnd();
      for(int k:order){
        double best=-1e100;int take=0;
        for(int id:by[k]){
          if(double(col[id].f)<=best)break;
          if(used.hit(col[id].mask))continue;
          double value=double(col[id].f);
          for(int r:col[id].ids)value-=factor*prices[r];
          if(value>best){best=value;take=id;}
        }
        chosen[k]=take;used=used|col[take].mask;
      }
      accept(chosen);
    }
  };
  greedy(100);publish();
  // An interrupted enumeration provides feasible columns, never an exact proof.
  if(!complete||out(until)){archive();return false;}
  for(int k=0;k<K;k++){
    vi keep;
    for(int id:by[k]){
      bool dominated=false;
      if(by[k].size()<=2000&&!out(until))for(int other:by[k])
        if(id!=other&&col[other].f>=col[id].f&&col[other].mask.sub(col[id].mask)){
          dominated=true;break;
        }
      if(!dominated)keep.push_back(id);
    }
    by[k]=move(keep);
  }
  bool large=R>256||K>24;
  double bestDual=1e100,alpha=1.8;
  double dualEnd=min(until,getTime()+min(large?20.0:0.5,(large?0.35:0.15)*(until-begin)));
  int stale=0;
  for(int it=0;it<5000&&!out(dualEnd);it++){
    vi count(R,0);double upper=accumulate(all(prices),0.0);
    for(int k=0;k<K;k++){
      double best=-1e100;int take=0;
      for(int id:by[k]){
        double value=double(col[id].f);
        for(int r:col[id].ids)value-=prices[r];
        if(value>best){best=value;take=id;}
      }
      upper+=best;chosen[k]=take;for(int r:col[take].ids)count[r]++;
    }
    accept(chosen);
    if(upper<bestDual-1e-7){bestDual=upper;saved=prices;stale=0;}else stale++;
    double norm=0;
    for(int r=0;r<R;r++)if(prices[r]>0||count[r]>1)norm+=(count[r]-1.0)*(count[r]-1.0);
    if(norm<1e-15||upper-double(record)<1e-7)break;
    if(it%(large?20:100)==0)greedy(8);
    if(stale>=100){alpha=max(0.002,alpha*0.75);prices=saved;stale=0;}
    double step=alpha*max(0.0,upper-double(record))/norm;
    for(int r=0;r<R;r++)prices[r]=min(double(total),max(0.0,prices[r]+step*(count[r]-1)));
  }
  // Quantize the dual to integers; all final B&B bounds are exact integer bounds.
  const ll B=1024;vl price(R,0);
  for(int r=0;r<R;r++)price[r]=max(0LL,llround(saved[r]*B));
  for(auto& z:col){for(int r:z.ids)z.pen+=price[r];z.red=z.f*B-z.pen;}
  vc<vi> profitRows;if(large)profitRows=by;
  for(auto& row:by)sort(all(row),[&](int a,int b){return col[a].red>col[b].red;});
  archive();
  ll root=accumulate(all(price),0LL);
  for(const auto& row:by)root+=col[row[0]].red;
  if(root/B<=record){publish();return true;}
  if(large){
    double end=min(until,getTime()+min(80.0,0.7*(until-begin)));
    vi current=bestChoice;ll value=record;uint64_t it=0;
    while(!out(end)){
      int k=int(rng()%unsigned(K));const auto& row=by[k];
      int width=min(sz(row),32+int(it%224));
      int id=row[size_t(rng()%unsigned(width))];
      vi trial=current,repairOrder;Mask used=col[id].mask;trial[k]=id;
      for(int j=0;j<K;j++)if(j!=k){
        if(used.hit(col[trial[j]].mask)||rnd()<0.06){trial[j]=0;repairOrder.push_back(j);}
        else used=used|col[trial[j]].mask;
      }
      shuffle(all(repairOrder),rng);
      for(int j:repairOrder){
        for(int x:profitRows[j])if(!used.hit(col[x].mask)){
          trial[j]=x;used=used|col[x].mask;break;
        }
      }
      ll next=0;for(int x:trial)next+=col[x].f;
      double temperature=max(1.0,0.025*double(record)/K);
      if(next>=value||rnd()<exp(double(next-value)/temperature)){current=trial;value=next;}
      accept(trial);
      if(++it%10000==0){current=bestChoice;value=record;}
    }
    publish();
  }
  if(large&&!out(TL-5.0)){
    auto start=getTime();
    double end=min(TL-5.0,start+175.0);
    while(!out(end)){
      ll previous=record;
      auto z=stableClique(col,by,price,bestChoice,record,end);
      accept(z.choice);publish();
      if(record!=z.value)throw logic_error("Clique master objective mismatch");
      cerr<<"event=stable_master benefit="<<record<<" proved="<<z.complete<<" rounds="<<z.rounds<<" cuts="<<z.cuts<<" nodes="<<z.nodes<<" seconds="<<getTime()-start<<" elapsed="<<getTime()<<"\n";
      if(z.complete)return true;
      if(record<=previous)break;
    }
  }
  double branchEnd=until;
  if(large)
    branchEnd=min(until,getTime()+min(3.0,0.1*(until-begin)));
  bool stopped=false;uint64_t nodes=0;
  function<void(uint64_t,Mask,ll,ll)> branch=[&](uint64_t assigned,Mask used,ll value,ll remaining){
    if(stopped)return;
    if((++nodes&255)==0&&out(branchEnd)){stopped=true;return;}
    if(assigned==(1ULL<<K)-1){if(value>record)accept(chosen);return;}
    vl top(K,0);ll upper=value*B+remaining;
    for(int k=0;k<K;k++)if(!(assigned>>k&1)){
      for(int id:by[k])if(!used.hit(col[id].mask)){top[k]=col[id].red;break;}
      upper+=top[k];
    }
    if(upper/B<=record)return;
    int taxi=-1,least=INT_MAX;
    for(int k=0;k<K;k++)if(!(assigned>>k&1)){
      int count=0;
      for(int id:by[k]){
        if(upper-top[k]+col[id].red<(record+1)*B)break;
        count+=!used.hit(col[id].mask);
      }
      if(count<least){least=count;taxi=k;}
    }
    if(!least)return;
    for(int id:by[taxi]){
      if(upper-top[taxi]+col[id].red<(record+1)*B)break;
      const auto& z=col[id];if(used.hit(z.mask))continue;
      chosen[taxi]=id;
      branch(assigned|(1ULL<<taxi),used|z.mask,value+z.f,remaining-z.pen);
      if(stopped)return;
    }
  };
  if(!out(until))branch(0,{},0,accumulate(all(price),0LL));else stopped=true;
  publish();return !stopped;
}
void input() {
  FastIn in;
  auto dimension=[&]() {
    ll x=in.next(); if(x<0||x>1000000)throw runtime_error("Invalid dimension"); return int(x);
  };
  K=dimension(); N=dimension(); M=dimension(); R=N+M; V=2*R+K;
  if(V>20000)throw runtime_error("Dense matrices exceed supported memory (V > 20000)");
  O.resize(K); Q.resize(K); rq.resize(R); ev.resize(2*R); pool.resize(K);
  auto vertex=[&]() {
    ll x=in.next(); if(x<1||x>V)throw runtime_error("Vertex outside 1..V"); return int(x-1);
  };
  auto nonnegative=[&]() { ll x=in.next(); if(x<0)throw runtime_error("Expected nonnegative data"); return x; };
  for(int k=0;k<K;k++) { O[k]=vertex(); Q[k]=nonnegative(); }
  vl rev;
  for(int r=0;r<R;r++) {
    auto& q=rq[r]; q.p=vertex(); q.d=vertex();
    if(!isP(r))q.w=nonnegative();
    q.e=nonnegative(); q.l=nonnegative();
    if(!isP(r)) { q.ed=nonnegative(); q.ld=nonnegative(); }
    q.s=nonnegative(); q.rev=nonnegative(); rev.push_back(q.rev);
  }
  tmx.resize(size_t(V)*V); cst.resize(size_t(V)*V);
  for(auto& x:tmx)x=nonnegative();
  for(auto& x:cst)x=nonnegative();
  for(int r=0;r<R;r++) {
    const auto& q=rq[r];
    if(isP(r))ev[2*r]={q.p,q.d,q.e,q.l,2*q.s+t(q.p,q.d),0,q.rev,c(q.p,q.d)};
    else {
      ev[2*r]={q.p,q.p,q.e,q.l,q.s,q.w,0,0};
      ev[2*r+1]={q.d,q.d,q.ed,q.ld,q.s,-q.w,q.rev,0};
    }
  }
  if(R) {
    nth_element(rev.begin(),rev.begin()+R/2,rev.end()); gs=max(1.0,double(rev[R/2]));
    vl durations;
    for(const auto& q:rq)durations.push_back(t(q.p,q.d)+2*q.s);
    nth_element(durations.begin(),durations.begin()+R/2,durations.end());
    gt=max(1.0,double(durations[R/2]));
  }
  ph.assign(size_t(R)*K,0.5); ans=emptySol();
}

// ---- SISR phase ----
static double envd(const char* n,double d){const char* e=getenv(n);return e&&*e?atof(e):d;}
// findMove without time checks/delay bookkeeping; each improving candidate position is skipped with prob thr/2^53 (blink).
static bool sisrMono=false;
struct CE{ll g[4];uint16_t i[4],j[4];int n=0;void add(int a,int b,ll x){if(n<4){g[n]=x;i[n]=uint16_t(a);j[n]=uint16_t(b);}n++;}};
static Ins sisrFind(const Sol& s,int r,int k,uint64_t thr,CE* rec=nullptr){
  Ins best; const Rt& z=s.a[k]; const Req& q=rq[r];
  if(q.w>Q[k])return best;
  int L=sz(z.a);
  auto blink=[&](){return (rng()>>11)<thr;};
  for(int i=0;i<=L;i++){
    int prev=i?ev[z.a[i-1]].v:O[k];
    ll before=i?z.tm[i-1]:0;
    if(i&&sisrMono&&before>q.l)break; // (4) tm[] non-decreasing when all t,dt>=0: later positions all fail begin>q.l
    ll begin=max(q.e,before+t(prev,q.p));
    if(begin>q.l)continue;
    ll oldEdge=i<L?c(prev,ev[z.a[i]].u):0;
    if(isP(r)){
      ll finish=begin+ev[2*r].dt;
      if(i<L&&finish+t(q.d,ev[z.a[i]].u)>z.lim[i])continue;
      ll delta=c(prev,q.p)+c(q.p,q.d)-oldEdge;
      if(i<L)delta+=c(q.d,ev[z.a[i]].u);
      ll gain=q.rev-delta;
      if(rec)rec->add(i,i,gain);
      if(gain>best.gain&&!blink())best={k,i,i,gain,0};
      continue;
    }
    ll baseLoad=i?z.w[i-1]:0;
    if(baseLoad+q.w>Q[k])continue;
    ll tm=begin+q.s; int u=q.p;
    ll pickupDelta=c(prev,q.p)-oldEdge;
    if(i<L)pickupDelta+=c(q.p,ev[z.a[i]].u);
    for(int j=i;j<=L;j++){
      ll delivery=max(q.ed,tm+t(u,q.d));
      if(delivery<=q.ld&&(j==L||delivery+q.s+t(q.d,ev[z.a[j]].u)<=z.lim[j])){
        ll delta;
        if(j==i){
          delta=c(prev,q.p)+c(q.p,q.d)-oldEdge;
          if(j<L)delta+=c(q.d,ev[z.a[j]].u);
        } else {
          delta=pickupDelta+c(u,q.d);
          if(j<L)delta+=c(q.d,ev[z.a[j]].u)-c(u,ev[z.a[j]].u);
        }
        ll gain=q.rev-delta;
        if(rec)rec->add(i,j,gain);
        if(gain>best.gain&&!blink())best={k,i,j,gain,0};
      }
      if(j==L)break;
      const Node& b=ev[z.a[j]];
      tm=max(b.l,tm+t(u,b.u));
      if(tm>b.r||z.w[j]+q.w>Q[k])break;
      tm+=b.dt; u=b.v;
    }
  }
  if(rec&&rec->n>4)rec->n=-1;
  return best;
}
// Exact assignment of the existing paths to taxis. With a fixed event
// order only the approach to the first event changes. The backward
// latest-start label and peak load characterize that taxi's feasibility.
static bool sisrAssignRoutes(Sol& s,double until){
  if(K<2||out(until))return false;
  vl cost(size_t(K)*K);
  for(int i=0;i<K;i++){
    if(out(until))return false;
    const auto& z=s.a[i];
    if(z.a.empty())continue;
    int from=ev[z.a.front()].u;
    ll peak=*max_element(all(z.w));
    for(int k=0;k<K;k++){
      if(c(O[k],from)>ll(1e15))return false; // overflow guard: keep Hungarian u/v far below INF
      cost[size_t(i)*K+k]=(peak<=Q[k]&&t(O[k],from)<=z.lim[0])
        ?c(O[k],from):INF/8;
    }
  }
  // Hungarian algorithm with deadline checks before committing anything.
  vl u(K+1),v(K+1),slack(K+1);
  vi colToRow(K+1),way(K+1);
  vc<char> usedCol(K+1);
  for(int i=1;i<=K;i++){
    if(out(until))return false;
    fill(all(slack),INF);
    fill(all(usedCol),0);
    colToRow[0]=i;
    int col=0;
    do{
      if(out(until))return false;
      usedCol[col]=1;
      int row=colToRow[col],next=0;
      ll delta=INF;
      for(int j=1;j<=K;j++)if(!usedCol[j]){
        ll w=cost[size_t(row-1)*K+j-1]-u[row]-v[j];
        if(w<slack[j]){slack[j]=w;way[j]=col;}
        if(slack[j]<delta){delta=slack[j];next=j;}
      }
      if(!next)return false;
      for(int j=0;j<=K;j++){
        if(usedCol[j]){u[colToRow[j]]+=delta;v[j]-=delta;}
        else slack[j]-=delta;
      }
      col=next;
    }while(colToRow[col]);
    do{
      int prev=way[col];
      colToRow[col]=colToRow[prev];
      col=prev;
    }while(col);
  }
  Sol candidate=emptySol();
  for(int k=0;k<K;k++){
    int i=colToRow[k+1]-1;
    if(i<0||cost[size_t(i)*K+k]>=INF/8||out(until))return false;
    candidate.a[k].a=s.a[i].a;
    if(!eval(k,candidate.a[k]))return false;
  }
  syncSol(candidate);
  if(candidate.f<=s.f)return false;
  s=move(candidate);
  return true;
}
static void sisrPhase(double Tsw){
  if(!R||!K||Tsw>=TL-0.05)return;
  const double cbar=envd("SISR_CBAR",10),Lmax=envd("SISR_LMAX",10),beta=envd("SISR_BETA",0.01);
  const int UNS=int(envd("SISR_UNS",5));
  const double swapRate=envd("SISR_SWAP",0.25);
  const uint64_t thr=uint64_t(envd("SISR_BLINK",0.01)*9007199254740992.0);
  const double T0=envd("SISR_T0",0.3)*gs,Tf=envd("SISR_TF",0.01)*gs;
  const uint64_t maxit=uint64_t(envd("SISR_MAXIT",0));
  const auto rt0=clk::now();
  const double t0=getTime();
  if(TL-t0<0.2)return;
  cerr<<"event=sisr_cfg gs="<<gs<<" T0="<<T0<<" Tf="<<Tf<<" cbar="<<cbar<<" Lmax="<<Lmax<<" uns="<<UNS<<" start="<<t0<<"\n";
  auto dist=[&](int a,int b)->ll{
    const auto& x=rq[a];const auto& y=rq[b];
    return min(t(x.p,y.p)+t(x.d,y.d),t(y.p,x.p)+t(y.d,x.d));
  };
  vc<vi> nb(R);
  for(int a=0;a<R;a++){
    vc<pair<ll,int>> v;v.reserve(R);
    for(int b=0;b<R;b++)v.emplace_back(a==b?-1:dist(a,b),b);
    sort(all(v));nb[a].reserve(R);
    for(auto&x:v)nb[a].push_back(x.second);
  }
  vi sig(K);{map<pair<int,ll>,int> m;for(int k=0;k<K;k++){auto key=make_pair(O[k],Q[k]);auto it=m.find(key);if(it==m.end())it=m.emplace(key,sz(m)).first;sig[k]=it->second;}}
  vi sigSeen(K+1,0),routeMark(K,0),reqMark(R,0);int sg=0,rm=0,qm=0;
  Sol cur=ans;Sol trial=cur;ll startBest=ans.f;
  sisrMono=true;for(ll x:tmx)if(x<0)sisrMono=false;for(const Node& b:ev)if(b.dt<0)sisrMono=false;
  // 2-way cache per (r,k): header {content ids, #feasible positions (-1 = >4, uncached)} kept apart from the payload so hits touch little memory
  struct CH{uint64_t st[2]={0,0};int8_t n[2]={0,0};};
  vc<CH> cacheH(size_t(R)*K);vc<CE> cacheP(size_t(R)*K*2);
  // route versions are content ids (exact: interned by full route content), so a route that returns to a previous content hits the cache again
  vl vcur(K),vtri(K);uint64_t stampCtr=0;unordered_map<string,uint64_t> intern;size_t internBytes=0;
  auto internRoute=[&](const Rt& z)->uint64_t{
    string key(reinterpret_cast<const char*>(z.a.data()),z.a.size()*sizeof(int));
    auto it=intern.find(key);if(it!=intern.end())return it->second;
    internBytes+=key.size()+64;intern.emplace(move(key),++stampCtr);return stampCtr;
  };
  for(int k=0;k<K;k++)vcur[k]=vtri[k]=internRoute(cur.a[k]);
  vi tch;vc<char> tchF(K,0),chgF(K,0);vi cutM(R,0);int cutC=0;Rt zs;
  auto touch=[&](int k){if(!tchF[k]){tchF[k]=1;tch.push_back(k);}vtri[k]=0;}; // 0 = content id not yet resolved
  // same result as eraseReq(trial,ids) (ids/f/routes), O(touched) instead of O(R+K) allocs; gone = requests that left the routes, unsorted
  auto eraseT=[&](const vi& ids,vi* gone){
    ++cutC;vi chg;
    for(int r:ids)if(trial.id[r]>=0){cutM[r]=cutC;int k=trial.id[r];if(!chgF[k]){chgF[k]=1;chg.push_back(k);}}
    vi old;
    for(int k:chg){
      chgF[k]=0;
      zs.a.clear();
      for(int x:trial.a[k].a)if(cutM[x/2]!=cutC)zs.a.push_back(x);
      while(!eval(k,zs)){
        int u=O[k],bad=-1;ll tm=0,load=0;
        for(int x:zs.a){
          const auto& b=ev[x];tm=max(b.l,tm+t(u,b.u));load+=b.w;
          if(tm>b.r||load<0||load>Q[k]){bad=x/2;break;}
          tm+=b.dt;u=b.v;
        }
        if(bad<0){zs=Rt();eval(k,zs);break;}
        zs.a.erase(remove_if(all(zs.a),[&](int x){return x/2==bad;}),zs.a.end());
      }
      for(int x:trial.a[k].a)if(!(x&1)){old.push_back(x/2);trial.id[x/2]=-1;}
      swap(trial.a[k],zs);
      for(int x:trial.a[k].a)if(!(x&1))trial.id[x/2]=k;
      touch(k);
    }
    trial.f=0;for(int k=0;k<K;k++)trial.f+=trial.a[k].f;
    if(gone)for(int r:old)if(trial.id[r]<0)gone->push_back(r);
  };
  auto findC=[&](int r,int k)->Ins{
    const size_t ix=size_t(r)*K+k;CH& e=cacheH[ix];
    uint64_t sv=vtri[k];if(!sv)sv=vtri[k]=internRoute(trial.a[k]);
    const int w=e.st[0]==sv?0:e.st[1]==sv?1:-1;
    if(w>=0){
      const int n=e.n[w];
      if(n<0)return sisrFind(trial,r,k,thr);
      Ins best;
      if(n){const CE& p=cacheP[ix*2+w];for(int x=0;x<n;x++)if(p.g[x]>best.gain&&!((rng()>>11)<thr))best={k,p.i[x],p.j[x],p.g[x],0};}
      return best;
    }
    const int vs=e.st[0]==vcur[k]?1:e.st[1]==vcur[k]?0:(e.st[0]<e.st[1]?0:1); // never evict the accepted route's entry
    CE tmp{};Ins res=sisrFind(trial,r,k,thr,&tmp);
    e.st[vs]=sv;e.n[vs]=int8_t(tmp.n);if(tmp.n>0)cacheP[ix*2+vs]=tmp;
    return res;
  };
  // same result as put(trial,r,p) without per-call vector allocations
  Rt zp;
  auto putT=[&](int r,const Ins& p){
    zp.a.clear();const vi& a=trial.a[p.k].a;
    for(int i=0;i<=sz(a);i++){if(i==p.i)zp.a.push_back(2*r);if(!isP(r)&&i==p.j)zp.a.push_back(2*r+1);if(i<sz(a))zp.a.push_back(a[i]);}
    if(!eval(p.k,zp))throw logic_error("Invalid insertion");
    if(zp.f-trial.a[p.k].f!=p.gain)throw logic_error("Stale insertion gain");
    trial.f+=zp.f-trial.a[p.k].f;
    swap(trial.a[p.k],zp);trial.id[r]=p.k;touch(p.k);
  };
  uint64_t its=0,acc=0,imp=0,pkImp=0;
  // SISR_PACK>0: archive accepted routes and recombine them every SISR_PACK s with Fleet's globalPack/pack (budget SISR_PACKT s)
  const double packEvery=envd("SISR_PACK",10),packT=envd("SISR_PACKT",1.0);const uint64_t psave=max<uint64_t>(1,uint64_t(envd("SISR_PSAVE",8)));
  double nextPk=t0+packEvery;
  const double patience=envd("SISR_PATIENCE",1e9);double lastImp=t0;int impLog=0;const char* why="deadline";
  double nextAssign=t0+1.0;
  int assignmentWins=0;
  ll assignmentGain=0;
  while(!out(TL)&&!(maxit&&its>=maxit)){
    double now=getTime();
    if(!maxit&&now>=nextAssign&&TL-now>0.06){
      nextAssign=now+2.5;
      Sol candidate=ans;
      if(sisrAssignRoutes(candidate,min(TL-0.025,now+0.035))){
        ++assignmentWins;
        assignmentGain+=candidate.f-ans.f;
        upd(candidate);
        lastImp=getTime();
        cur=move(candidate);trial=cur;
        for(int k=0;k<K;k++)vcur[k]=vtri[k]=internRoute(cur.a[k]);
      }
    }
    if(now-lastImp>patience){why="patience";break;} // time since max(sisrStart,lastImprovement)
    double prog=maxit?double(its)/double(maxit):min(1.0,(now-t0)/max(1e-9,TL-t0));
    double T=T0*pow(Tf/T0,prog);
    its++;
    if(packEvery>0&&now>=nextPk){
      const ll before=ans.f;
      // SISR_PKMODE: 0 = adopt pack result if > cur (resets to ans), 1 = adopt only on a strict pack improvement, 2 = control: no pack, reset cur to ans
      const int pkMode=int(envd("SISR_PKMODE",1));
      Sol p=pkMode==2?ans:(R>120&&R<=512&&K>=8)?globalPack(min(TL,now+packT)):pack(min(TL,now+packT));
      if(ans.f>before){pkImp++;imp++;lastImp=getTime();if(impLog++<2000)cerr<<"event=sisr_pack_imp t="<<getTime()<<" best="<<ans.f<<"\n";}
      if(p.f>cur.f&&(pkMode!=1||ans.f>before)){cur=move(p);trial=cur;for(int k=0;k<K;k++)vcur[k]=vtri[k]=internRoute(cur.a[k]);}
      nextPk=getTime()+packEvery;
      if(out(TL))break;
    }
    if(intern.size()>200000||internBytes>(size_t(8)<<20)){intern.clear();internBytes=0;for(int k=0;k<K;k++)vcur[k]=vtri[k]=internRoute(cur.a[k]);} // bound memory; ids are never reused

    vi removed,cand;
    bool swapRound=swapRate>0&&rnd()<swapRate;
    if(swapRound){
      int incoming=-1;
      for(int tries=0;tries<32;tries++){int r=rd(R);if(cur.id[r]<0){incoming=r;break;}}
      if(incoming>=0){
        Ins winner;int victim=-2;ll bestF=BAD;
        for(int k=0;k<K;k++){
          Ins p=sisrFind(cur,incoming,k,0);
          if(p.ok()&&p.gain>0&&cur.f+p.gain>bestF){bestF=cur.f+p.gain;winner=p;victim=-1;}
        }
        int tried=0;const int active=sz(used(cur));
        for(int v:nb[incoming]){
          if(cur.id[v]<0)continue;
          if(tried++>=12||out(TL))break;
          int k=cur.id[v];Sol probe=cur;eraseReq(probe,vi{v});
          // Keep this an atomic one-for-one exchange, even on nonmetric inputs.
          if(sz(used(probe))!=active-1)continue;
          Ins p=sisrFind(probe,incoming,k,0);
          if(p.ok()&&p.gain>0&&probe.f+p.gain>bestF){
            bestF=probe.f+p.gain;winner=p;victim=v;
          }
        }
        if(victim!=-2){
          if(victim>=0)eraseT(vi{victim},&removed);
          putT(incoming,winner);cand.push_back(incoming);
        }
      }
    }else{
    vi sv=used(trial);
    int seed=sv.empty()?rd(R):sv[rd(sz(sv))];

    if(!sv.empty()){
      int usedRoutes=0;for(int k=0;k<K;k++)usedRoutes+=!trial.a[k].a.empty();
      double lsmax=min(Lmax,double(sz(sv))/max(1,usedRoutes));
      double ksmax=4*cbar/(1+lsmax)-1;
      int ks=max(1,int(1+rnd()*max(0.0,ksmax)));
      ++rm;vi cut;int ruined=0;
      for(int cnd:nb[seed]){
        if(ruined>=ks)break;
        int k=trial.id[cnd];if(k<0||routeMark[k]==rm)continue;
        routeMark[k]=rm;ruined++;
        vi seq;int pos=-1;
        for(int x:trial.a[k].a)if(!(x&1)){if(x/2==cnd)pos=sz(seq);seq.push_back(x/2);}
        int n=sz(seq);
        int l=max(1,min(n,int(1+rnd()*min(lsmax,double(n)))));
        if(l<n&&rnd()<0.5){ // split string: remove l of a window of l+m, keeping m consecutive inside
          int m=1;while(l+m<n&&rnd()>beta)m++;
          int w=l+m,lo=max(0,pos-w+1),hi=min(pos,n-w),start=lo+rd(hi-lo+1),ps=rd(l+1);
          for(int i=0;i<w;i++)if(i<ps||i>=ps+m)cut.push_back(seq[start+i]);
        } else {
          int lo=max(0,pos-l+1),hi=min(pos,n-l),start=lo+rd(hi-lo+1);
          for(int i=0;i<l;i++)cut.push_back(seq[start+i]);
        }
      }
      eraseT(cut,&removed);sort(all(removed));
      vi neg; // routes left with negative profit are dropped entirely
      for(int k=0;k<K;k++)if(trial.a[k].f<0)for(int x:trial.a[k].a)if(!(x&1))neg.push_back(x/2);
      if(!neg.empty()){eraseT(neg,nullptr);for(int r:neg)removed.push_back(r);}
    }
    cand=removed;++qm;
    for(int r:cand)reqMark[r]=qm;
    int extra=0;
    for(int cnd:nb[seed]){
      if(extra>=UNS)break;
      if(trial.id[cnd]<0&&reqMark[cnd]!=qm){cand.push_back(cnd);reqMark[cnd]=qm;extra++;}
    }
    double u=rnd()*10;
    if(u<4)shuffle(all(cand),rng);
    else if(u<8)sort(all(cand),[&](int a,int b){return rq[a].rev>rq[b].rev;});
    else sort(all(cand),[&](int a,int b){return dist(seed,a)<dist(seed,b);});
    for(int r:cand){
      Ins best;++sg;
      for(int k=0;k<K;k++){
        if(rq[r].w>Q[k])continue;
        if(trial.a[k].a.empty()){if(sigSeen[sig[k]]==sg)continue;sigSeen[sig[k]]=sg;}
        Ins v=findC(r,k);if(v.gain>best.gain)best=v;
      }
      if(best.ok()&&best.gain>0)putT(r,best);
    }
    }
    ll delta=trial.f-cur.f;
    bool accept=delta>=0||rnd()<exp(max(-700.0,double(delta)/T));
    if(trial.f>ans.f){ll b=ans.f;upd(trial);if(ans.f>b){imp++;lastImp=now;if(impLog++<2000)cerr<<"event=sisr_imp t="<<now<<" best="<<ans.f<<"\n";}}
    if(accept){
      for(int k:tch)for(int x:cur.a[k].a)if(!(x&1))cur.id[x/2]=-1;
      for(int k:tch){if(!vtri[k])vtri[k]=internRoute(trial.a[k]);cur.a[k]=trial.a[k];vcur[k]=vtri[k];for(int x:cur.a[k].a)if(!(x&1))cur.id[x/2]=k;}
      cur.f=trial.f;acc++;
      if(packEvery>0&&acc%psave==0)for(int k:tch)save(k,cur.a[k]);
    } else {
      for(int k:tch)for(int x:trial.a[k].a)if(!(x&1))trial.id[x/2]=-1;
      for(int k:tch){trial.a[k]=cur.a[k];vtri[k]=vcur[k];for(int x:trial.a[k].a)if(!(x&1))trial.id[x/2]=k;}
      trial.f=cur.f;
    }
    for(int k:tch)tchF[k]=0;
    tch.clear();
  }
  if(maxit&&its>=maxit&&why[0]=='d')why="maxit";
  cerr<<"event=assignment wins="<<assignmentWins<<" gain="<<assignmentGain<<"\n";
  const double te=getTime();
  cerr<<"event=sisr_exit reason="<<why<<" t="<<te<<"\n";
  cerr<<"event=sisr iters="<<its<<" accepted="<<acc<<" improvements="<<imp<<" pack_imp="<<pkImp<<" start_best="<<startBest<<" end_best="<<ans.f<<" secs="<<te-t0<<" real_secs="<<chrono::duration<double>(clk::now()-rt0).count()<<"\n";
}
static Sol gSafe; // best valid solution snapshot taken before the SISR phase (fallback if ans is left corrupt by an exception)
void solve() {
  if(!R||!K||out(TL))return;
  if(all_of(all(rq),[](const Req& q){return q.rev==0;}))return;
  vi order(R); iota(all(order),0);
  sort(all(order),[&](int a,int b){return rq[a].rev>rq[b].rev;});
  Sol seed=ans;
  double bootstrapEnd=min(TL,getTime()+min(0.5,0.025*TL));
  for(int r:order){
    if(out(bootstrapEnd))break;
    auto p=bestMove(seed,r,bootstrapEnd);
    if(p.ok()&&p.gain>0){put(seed,r,p);upd(seed);}
  }
  if(ex&&R<=10&&K<=12) {
    double budget=min(12.0,max(0.01,0.25*TL));
    if(exactDfs(min(TL,getTime()+budget)))return;
  } else if(ex&&R<=512&&K<=63) {
    double budget=R>256||K>24?min(120.0,max(0.01,0.8*TL)):min(12.0,max(0.01,0.15*TL));
    if(exactSparse(min(TL,getTime()+budget)))return;
  }
  Sol current=ans;
  double initialEnd=min(TL,getTime()+min(5.0,max(0.01,0.08*TL)));
  repair(current,1,{},initialEnd,true); upd(current);
  for(int k=0;k<K;k++)save(k,current.a[k]);
  ALNS destruction(7),reconstruction(8);
  double nextPack=getTime()+min(4.0,max(0.04,0.04*TL));
  double lastBest=getTime(); ll previousBest=ans.f;
  const double Tsw=min(TL,envd("SISR_START",0.25)*TL);
  const double minNodes=envd("SISR_MINNODES",0);
  auto stops=[&]{ll n=0;for(const auto& z:ans.a)for(int x:z.a)n+=isP(x/2)?2:1;return n;};
  const double minLen=envd("SISR_MINLEN",0);
  auto longRoutes=[&]{ll n=0;for(const auto& z:ans.a)n+=!z.a.empty();return double(stops())>=minLen*double(max(1LL,n));};
  const uint64_t warmit=uint64_t(envd("SISR_WARMIT",0));
  bool decided=false;
  while(!out(TL)) {
    if(!decided&&(warmit?iter>=warmit:getTime()>=Tsw)){
      decided=true; // the gate is evaluated exactly once
      const bool go=double(stops())>=minNodes&&longRoutes();
      cerr<<"event=gate open="<<go<<" stops="<<stops()<<" t="<<getTime()<<"\n";
      if(go){
        gSafe=ans;
        try{sisrPhase(Tsw);}catch(...){ // SISR caches are locals of sisrPhase: already released by unwinding
          bool good=false;try{good=check(ans);}catch(...){}
          if(!good)swap(ans,gSafe); // no allocation
          cerr<<"event=sisr_exception good="<<good<<"\n";
        }
        if(out(TL))break;
        current=ans;for(int k=0;k<K;k++)save(k,current.a[k]);
        lastBest=getTime();previousBest=ans.f;
        continue;
      }
    }
    iter++;
    double remaining=TL-getTime();
    double slice=min(remaining,max(0.015,min(1.0,0.006*TL)));
    double until=min(TL,getTime()+slice);
    bool polish=iter%3==0;
    double repairEnd=polish?getTime()+max(0.0,until-getTime())*0.65:until;
    int d=destruction.pick(rng),r=reconstruction.pick(rng);
    Sol trial=current;
    int active=int(used(trial).size());
    double stagnation=getTime()-lastBest;
    double fraction=stagnation>max(1.0,0.05*TL)?0.15+0.55*rnd():0.05+0.30*rnd();
    int amount=max(1,min(120,int(ceil(active*fraction))));
    auto removed=ruin(trial,d,amount,repairEnd);
    repair(trial,r,removed,repairEnd,false,&current.id);
    if(polish&&!out(until))ls(trial,until);
    upd(trial);
    ll delta=trial.f-current.f;
    double cycle=fmod(getTime()/max(0.1,TL),0.22)/0.22;
    double temperature=max(0.02,0.10*gs*pow(0.015,cycle));
    bool accepted=delta>=0||rnd()<exp(max(-700.0,double(delta)/temperature));
    double val=0.1;
    if(ans.f>previousBest) { val=12; previousBest=ans.f; lastBest=getTime(); }
    else if(delta>0)val=5;
    else if(accepted)val=1;
    destruction.val(d,val); reconstruction.val(r,val);
    
    if(iter%3==0)for(int k=0;k<K;k++)
      if(trial.a[k].a!=current.a[k].a)save(k,trial.a[k]);
    if(accepted) {
      current=move(trial);
      for(int k=0;k<K;k++)save(k,current.a[k]);
    }
    if(iter%40==0) {
      destruction.update(); reconstruction.update();
      learn(iter%120==0?current:ans);
    }
    if(getTime()>=nextPack&&!out(TL)) {
      const double packEnd=min(TL,getTime()+min(1.2,max(0.01,0.01*TL)));
      Sol packed;
      if(R>120&&R<=512&&K>=8&&iter%3!=0)packed=globalPack(packEnd);
      else packed=pack(packEnd);
      if(packed.f>current.f||rnd()<0.15)current=move(packed);
      if(!out(TL))ls(current,min(TL,getTime()+min(0.8,0.005*TL)));
      upd(current);
      nextPack=getTime()+min(5.0,max(0.06,0.04*TL));
    }
    if(stagnation>max(1.0,0.12*TL)&&iter%30==0) {
      current=rnd()<0.7?ans:emptySol();
      if(current.f==0)repair(current,3,{},min(TL,getTime()+slice),true);
    }
  }
  cerr<<"event=alns_end t="<<getTime()<<" best="<<ans.f<<" stops="<<stops()<<"\n";
}
void print() {
  if(!check(ans))throw logic_error("Final solution failed independent structural validation");
  cout<<ans.f<<'\n';
  for(const auto& route:ans.a) {
    size_t cnt=0; for(int x:route.a)cnt+=isP(x/2)?2:1;
    cout<<cnt;
    for(int x:route.a) {
      const auto& q=rq[x/2];
      cout<<' '<<((x&1)?q.d:q.p)+1;
      if(isP(x/2))cout<<' '<<q.d+1;
    }
    cout<<'\n';
  }
}
static void recoverAns(){ // an exception escaped solve(): keep the best valid solution available
  bool ok=false;try{ok=check(ans);}catch(...){}
  if(ok)return;
  try{if(sz(gSafe.a)==K&&check(gSafe)){swap(ans,gSafe);return;}}catch(...){}
  try{ans=emptySol();}catch(...){}
}
void init(double sec,uint64_t seed,bool use) {
  rng.seed(seed);
  st=clk::now();
  { const char* e=getenv("SISR_WARMIT"); detClock=e&&atof(e)>0; }
  ex=use;
  TL=max(0.0,sec-min(0.15,sec*0.02));
}

int main(int argc,char** argv) {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);
  double sec=295;
  uint64_t seed=42;
  bool use=true;
  try {
    for(int i=1;i<argc;i++) {
      string arg=argv[i];
      if(arg=="--time"&&i+1<argc)sec=stod(argv[++i]);
      else if(arg=="--seed"&&i+1<argc)seed=stoull(argv[++i]);
      else if(arg=="--no-exact")use=false;
      else return 1;
    }
    if(!isfinite(sec)||sec<0||sec>298)return 1;
    init(sec,seed,use);
    input();
  } catch(...) { return 1; } // malformed input / bad args: as before
  try { solve(); } catch(...) { recoverAns(); }
  try { print(); } catch(...) { return 1; }
  cout.flush(); fflush(stdout);
  std::_Exit(0); // skip teardown of large containers
}
