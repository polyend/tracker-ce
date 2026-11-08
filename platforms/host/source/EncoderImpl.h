#ifndef SOURCE_ENCODERIMPL_H_
#define SOURCE_ENCODERIMPL_H_

#include "Platform.h"

class EncoderImpl : public Encoder
{
  public:
    virtual void setAcceleration(int acceleration) override { (void)acceleration; }
    virtual void setResolution(int resolution) override { (void)resolution; }

    void turn(int detents);
    void update();

  private:
    int pending = 0;
};

#endif
