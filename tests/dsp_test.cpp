#include "../Source/FusionDsp.h"
#include <cstdio>
#include <vector>
using namespace fd;
static double goertzel(const std::vector<double>& x,double f,double fs){double w=2*kPi*f/fs,c=2*std::cos(w),s1=0,s2=0;for(double v:x){double s=v+c*s1-s2;s2=s1;s1=s;}double re=s1-s2*std::cos(w),im=s2*std::sin(w);return 2*std::sqrt(re*re+im*im)/x.size();}
int main(){
  const double fs=192000; int bad=0;
  for(int analog=0;analog<2;++analog)for(double dr:{0.0,10.0,20.0,30.0})for(double f0:{100.0,1000.0}){
    Channel c;c.prepare(fs,0);Params p;p.analog=analog;p.driveDb=dr;c.setParams(p);
    const double amp=0.25;std::vector<double> o;int N=(int)(fs/f0*20);
    for(int n=0;n<N+(int)fs/4;++n){double y=c.process(amp*std::sin(2*kPi*f0*n/fs));if(!std::isfinite(y))bad++;if(n>=(int)fs/4)o.push_back(y);}
    // use integer periods
    int per=(int)std::round(fs/f0);o.resize(per*(int)(o.size()/per));
    double h1=goertzel(o,f0,fs),thd=0;std::printf("%s drive %4.0fdB f=%5.0f  gain %+6.1fdB  H2 %6.1f H3 %6.1f H4 %6.1f H5 %6.1f dBc\n",analog?"ANALOG":"VINTAGE",dr,f0,20*std::log10(h1/amp/std::pow(10,dr/20)+1e-12),
      20*std::log10(goertzel(o,2*f0,fs)/h1+1e-12),20*std::log10(goertzel(o,3*f0,fs)/h1+1e-12),20*std::log10(goertzel(o,4*f0,fs)/h1+1e-12),20*std::log10(goertzel(o,5*f0,fs)/h1+1e-12));
  }
  // stress: noise at +12 dBFS, max everything
  for(int analog=0;analog<2;++analog){Channel c;c.prepare(96000,1);Params p;p.analog=analog;p.driveDb=30;p.tone=1;p.character=1;p.knee=1;p.iron=1;p.sag=1;p.slew=1;p.drift=1;c.setParams(p);
    uint32_t s=1;double mx=0;for(int n=0;n<2000000;++n){s=s*1664525u+1013904223u;double x=4.0*((s>>8)/8388608.0-1.0);double y=c.process(x);if(!std::isfinite(y))bad++;mx=std::max(mx,std::abs(y));}
    std::printf("stress %s peak %.2f\n",analog?"analog":"vintage",mx);}
  std::printf("non-finite: %d\n",bad);return bad!=0;}
