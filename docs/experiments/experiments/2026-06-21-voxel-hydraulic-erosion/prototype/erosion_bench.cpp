#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

constexpr int kGridW = 128;
constexpr int kGridH = 128;
constexpr float kPipeArea = 1.0f;
constexpr float kPipeLen  = 1.0f;
constexpr float kDt       = 0.01f;
constexpr float kEvapRate = 0.001f;
constexpr float kSedCap   = 0.1f;
constexpr float kDissolve = 0.01f;
constexpr float kDeposit  = 0.01f;
constexpr int   kMaxDroplets = 10000;
constexpr float kSlopeMax = 1.2f;
constexpr std::array kSeeds = {1u, 7u, 42u, 1234u, 31337u};

struct Stats { double mean=0, median=0, p95=0, p99=0, stddev=0, min=0, max=0; };
Stats computeStats(const std::vector<double>& s) {
    if (s.empty()) return {};
    auto sorted = s;
    std::sort(sorted.begin(), sorted.end());
    double sum = 0;
    for (auto v : s) sum += v;
    Stats st;
    st.mean = sum / s.size();
    st.median = sorted[s.size() / 2];
    st.p95 = sorted[static_cast<size_t>(s.size() * 0.95)];
    st.p99 = sorted[static_cast<size_t>(s.size() * 0.99)];
    st.min = sorted.front();
    st.max = sorted.back();
    double var = 0;
    for (auto v : s) var += (v - st.mean) * (v - st.mean);
    st.stddev = std::sqrt(var / s.size());
    return st;
}

struct Vec2 { float x, y; };
inline float hash(Vec2 p) {
    auto h = std::hash<float>{}(p.x) ^ (std::hash<float>{}(p.y) * 0x9e3779b9);
    return (h & 0xffffff) / float(0x1000000);
}
inline float lerp(float a, float b, float t) { return a + t * (b - a); }
inline float smoothstep(float t) { return t * t * (3 - 2 * t); }
float noise(Vec2 p) {
    int ix = int(std::floor(p.x)), iy = int(std::floor(p.y));
    float fx = p.x - ix, fy = p.y - iy;
    float sx = smoothstep(fx), sy = smoothstep(fy);
    float n00 = hash({float(ix), float(iy)});
    float n10 = hash({float(ix+1), float(iy)});
    float n01 = hash({float(ix), float(iy+1)});
    float n11 = hash({float(ix+1), float(iy+1)});
    return lerp(lerp(n00, n10, sx), lerp(n01, n11, sx), sy);
}
float fbm(Vec2 p, int oct=4) {
    float v=0, a=1, f=1, t=0;
    for (int i=0;i<oct;++i){v+=a*noise({p.x*f,p.y*f});t+=a;a*=0.5f;f*=2;}
    return v/t;
}

using Heightmap = std::vector<float>;
enum Scene{FLAT=0,GENTLE_HILLS,STEEP_MOUNTAINS,RIVER_VALLEY,MIXED,NUM_SCENES};
const char* sceneName(int s) {
    switch(s){
        case FLAT:return"flat";case GENTLE_HILLS:return"gentle_hills";
        case STEEP_MOUNTAINS:return"steep_mountains";case RIVER_VALLEY:return"river_valley";
        case MIXED:return"mixed";default:return"unknown";
    }
}

Heightmap genTerrain(int scene, unsigned seed) {
    Heightmap h(kGridW*kGridH);
    for(int y=0;y<kGridH;++y)for(int x=0;x<kGridW;++x){
        Vec2 p{float(x)/kGridW*8,float(y)/kGridH*8};
        float n=fbm(p,6);
        switch(scene){
            case FLAT:h[y*kGridW+x]=0.5f+(n-0.5f)*0.1f;break;
            case GENTLE_HILLS:h[y*kGridW+x]=0.3f+n*0.4f;break;
            case STEEP_MOUNTAINS:h[y*kGridW+x]=0.1f+n*0.9f;break;
            case RIVER_VALLEY:{
                float d=std::abs(float(x)/kGridW-0.5f);
                float v=1.0f-std::pow(d*2,0.3f);
                h[y*kGridW+x]=0.2f+n*0.4f+v*0.2f;
                if(d<0.05f)h[y*kGridW+x]=0.1f;
                break;
            }
            case MIXED:{
                float r=float(x)/kGridW;
                if(r<0.33f)h[y*kGridW+x]=0.5f+(n-0.5f)*0.1f;
                else if(r<0.66f)h[y*kGridW+x]=0.3f+n*0.4f;
                else h[y*kGridW+x]=0.1f+fbm({p.x*1.5f,p.y*1.5f},6)*0.9f;
                break;
            }
        }
    }
    return h;
}

void stratA(Heightmap&,int){}

void stratB(Heightmap& h,int n){
    int W=kGridW,H=kGridH;
    auto idx=[&](int x,int y){return y*W+x;};
    for(int i=0;i<n;++i)for(int d=0;d<kMaxDroplets/n;++d){
        int x=rand()%W,y=rand()%H;float sed=0,water=1;
        for(int st=0;st<100;++st){
            int nx=x,ny=y;float best=h[idx(x,y)],dh=0;
            auto ts=[&](int tx,int ty){
                if(tx<0||tx>=W||ty<0||ty>=H)return;
                float hh=h[idx(tx,ty)];
                if(hh<best){best=hh;nx=tx;ny=ty;dh=h[idx(x,y)]-hh;}
            };
            ts(x-1,y);ts(x+1,y);ts(x,y-1);ts(x,y+1);
            if(nx==x&&ny==y)break;
            float cap=kSedCap*dh*water;
            if(cap>sed){float e=std::min((cap-sed)*kDissolve,h[idx(x,y)]*0.1f);h[idx(x,y)]-=e;sed+=e;}
            else{float d=(sed-cap)*kDeposit;h[idx(x,y)]+=d;sed-=d;}
            x=nx;y=ny;water*=(1-kEvapRate);if(water<0.01f)break;
        }
    }
}

void stratC(Heightmap& h,int n){
    int W=kGridW,H=kGridH;
    auto idx=[&](int x,int y){return y*W+x;};
    std::vector<float> w(W*H,0),sed(W*H,0);
    for(int i=0;i<n;++i){
        for(auto&v:w)v+=0.001f;
        std::vector<float> out(W*H,0);
        for(int y=0;y<H;++y)for(int x=0;x<W;++x){
            float tot=0;
            for(auto[dxx,dyy]:{std::pair{-1,0},{1,0},{0,-1},{0,1}}){
                int nx=x+dxx,ny=y+dyy;
                if(nx<0||nx>=W||ny<0||ny>=H)continue;
                float dh=(h[idx(x,y)]+w[idx(x,y)])-(h[idx(nx,ny)]+w[idx(nx,ny)]);
                if(dh>0)tot+=std::min(dh*kPipeArea/kPipeLen*kDt,w[idx(x,y)]*0.5f);
            }
            out[idx(x,y)]=std::min(tot,w[idx(x,y)]);
        }
        for(int y=0;y<H;++y)for(int x=0;x<W;++x){
            float o=out[idx(x,y)];if(o<=0)continue;
            float th=0;
            for(auto[dxx,dyy]:{std::pair{-1,0},{1,0},{0,-1},{0,1}}){
                int nx=x+dxx,ny=y+dyy;
                if(nx<0||nx>=W||ny<0||ny>=H)continue;
                float dh=(h[idx(x,y)]+w[idx(x,y)])-(h[idx(nx,ny)]+w[idx(nx,ny)]);
                if(dh>0)th+=dh;
            }
            if(th<=0)continue;
            float so=sed[idx(x,y)]*(o/(w[idx(x,y)]+1e-6f));
            w[idx(x,y)]-=o;sed[idx(x,y)]-=so;
            for(auto[dxx,dyy]:{std::pair{-1,0},{1,0},{0,-1},{0,1}}){
                int nx=x+dxx,ny=y+dyy;
                if(nx<0||nx>=W||ny<0||ny>=H)continue;
                float dh=(h[idx(x,y)]+w[idx(x,y)])-(h[idx(nx,ny)]+w[idx(nx,ny)]);
                if(dh<=0)continue;
                float fr=dh/th;w[idx(nx,ny)]+=o*fr;sed[idx(nx,ny)]+=so*fr;
            }
        }
        for(int y=0;y<H;++y)for(int x=0;x<W;++x){
            float ww=w[idx(x,y)],ss=sed[idx(x,y)],cap=kSedCap*ww;
            float sx=0,sy=0;int c=0;
            if(x>0){sx+=h[idx(x,y)]-h[idx(x-1,y)];c++;}
            if(x<W-1){sx+=h[idx(x+1,y)]-h[idx(x,y)];c++;}
            if(y>0){sy+=h[idx(x,y)]-h[idx(x,y-1)];c++;}
            if(y<H-1){sy+=h[idx(x,y+1)]-h[idx(x,y)];c++;}
            if(c)cap*=(1+std::sqrt(sx*sx+sy*sy)*2);
            if(cap>ss&&ww>0.01f){float e=std::min((cap-ss)*kDissolve,h[idx(x,y)]*0.05f);h[idx(x,y)]-=e;sed[idx(x,y)]+=e;}
            else if(ss>cap){float d=(ss-cap)*kDeposit;h[idx(x,y)]+=d;sed[idx(x,y)]-=d;}
            w[idx(x,y)]*=(1-kEvapRate);
        }
    }
}

struct GPUModel{double timeUs;double vramMiB;};
GPUModel stratD(){
    int c=kGridW*kGridH;
    double mem=4.0*c*4.0*2.0,bw=448e9*0.35,tMem=mem/bw*1e6;
    double alu=c*300.0/12.7e12*1e6;
    double launch=8.0,total=tMem+alu+launch;
    double vram=4.0*c*4.0/(1024.0*1024.0);
    return{total,vram};
}

void stratE(Heightmap& h,int n){
    int W=kGridW,H=kGridH;
    auto idx=[&](int x,int y){return y*W+x;};
    for(int i=0;i<n;++i){
        Heightmap c=h;
        for(int y=0;y<H;++y)for(int x=0;x<W;++x){
            float ms=0;
            for(auto[dxx,dyy]:{std::pair{-1,-1},{0,-1},{1,-1},{-1,0},{1,0},{-1,1},{0,1},{1,1}}){
                int nx=x+dxx,ny=y+dyy;
                if(nx<0||nx>=W||ny<0||ny>=H)continue;
                float d=(h[idx(x,y)]-h[idx(nx,ny)])/std::sqrt(float(dxx*dxx+dyy*dyy));
                if(d>ms)ms=d;
            }
            if(ms>kSlopeMax){
                float ex=(ms-kSlopeMax)*0.1f;c[idx(x,y)]-=ex;
                float ts=0;int nc=0;
                for(auto[dxx,dyy]:{std::pair{-1,-1},{0,-1},{1,-1},{-1,0},{1,0},{-1,1},{0,1},{1,1}}){
                    int nx=x+dxx,ny=y+dyy;
                    if(nx<0||nx>=W||ny<0||ny>=H)continue;
                    float d=h[idx(x,y)]-h[idx(nx,ny)];
                    if(d>0)ts+=d;if(d>=0)nc++;
                }
                if(ts>0){
                    for(auto[dxx,dyy]:{std::pair{-1,-1},{0,-1},{1,-1},{-1,0},{1,0},{-1,1},{0,1},{1,1}}){
                        int nx=x+dxx,ny=y+dyy;
                        if(nx<0||nx>=W||ny<0||ny>=H)continue;
                        float d=h[idx(x,y)]-h[idx(nx,ny)];
                        if(d>0)c[idx(nx,ny)]+=ex*d/ts;
                    }
                }else if(nc>0){
                    for(auto[dxx,dyy]:{std::pair{-1,-1},{0,-1},{1,-1},{-1,0},{1,0},{-1,1},{0,1},{1,1}}){
                        int nx=x+dxx,ny=y+dyy;
                        if(nx>=0&&nx<W&&ny>=0&&ny<H&&h[idx(x,y)]-h[idx(nx,ny)]>=0)
                            c[idx(nx,ny)]+=ex/nc;
                    }
                }
            }
        }
        h=c;
    }
}

float psnr(const Heightmap& a,const Heightmap& b){
    if(a.size()!=b.size())return 0;
    float mse=0;
    for(size_t i=0;i<a.size();++i){float d=a[i]-b[i];mse+=d*d;}
    mse/=a.size();if(mse<1e-10f)return 100;
    return 10*std::log10(1.0f/mse);
}

int main(){
    std::printf("strategy,scene,seed,time_mean_us,time_median_us,time_p95_us,time_p99_us,"
                "time_std_us,time_min_us,time_max_us,psnr_vs_reference_db,psnr_vs_raw_db,iterations\n");
    const char*n[]={"A_NoErosion","B_CPUParticleDroplet","C_CPUPipeModel",
                    "D_GPUPipeModelAnalytical","E_SimplifiedSlopeMethod"};
    constexpr int kNS=5,kMI=200,kWU=5,kRuns=5;

    std::vector<Heightmap> tr(NUM_SCENES*kSeeds.size());
    std::vector<Heightmap> rf(NUM_SCENES*kSeeds.size());
    std::vector<Heightmap> rw(NUM_SCENES*kSeeds.size());
    for(int s=0;s<NUM_SCENES;++s)for(size_t si=0;si<kSeeds.size();++si){
        tr[s*kSeeds.size()+si]=genTerrain(s,kSeeds[si]);
        rw[s*kSeeds.size()+si]=tr[s*kSeeds.size()+si];
        auto ref=tr[s*kSeeds.size()+si];stratC(ref,500);
        rf[s*kSeeds.size()+si]=ref;
    }

    for(int st=0;st<kNS;++st)for(int s=0;s<NUM_SCENES;++s)for(size_t si=0;si<kSeeds.size();++si){
        int idx=s*kSeeds.size()+si;
        if(st==3){
            auto g=stratD();std::vector<double> t(1,g.timeUs);auto stt=computeStats(t);
            std::printf("%s,%s,%u,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.2f,%.2f,%d\n",
                n[st],sceneName(s),kSeeds[si],stt.mean,stt.median,stt.p95,stt.p99,
                stt.stddev,stt.min,stt.max,0.0f,0.0f,1);
            continue;
        }
        std::vector<double> times;Heightmap final;
        for(int run=0;run<kRuns;++run){
            auto h=tr[idx];auto t0=std::chrono::high_resolution_clock::now();
            switch(st){case 0:break;case 1:stratB(h,kMI);break;case 2:stratC(h,kMI);break;case 4:stratE(h,kMI);break;}
            auto t1=std::chrono::high_resolution_clock::now();
            double us=std::chrono::duration<double,std::micro>(t1-t0).count();
            times.push_back(us/kMI);final=h;
        }
        auto stt=computeStats(times);
        float pR=psnr(final,rf[idx]),pW=psnr(final,rw[idx]);
        std::printf("%s,%s,%u,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.2f,%.2f,%d\n",
            n[st],sceneName(s),kSeeds[si],stt.mean,stt.median,stt.p95,stt.p99,
            stt.stddev,stt.min,stt.max,pR,pW,kMI);
    }
    return 0;
}
