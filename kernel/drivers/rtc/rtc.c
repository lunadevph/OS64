#include "rtc.h"
#include "io.h"

typedef struct {
    uint8_t second,minute,hour,day,month,year,century;
} cmos_time_t;

static uint8_t cmos_read(uint8_t index)
{
    /* Keep NMIs disabled only for the index/data transaction. */
    outb(0x70,(uint8_t)(0x80u|index));
    uint8_t value=inb(0x71);
    outb(0x70,0);
    return value;
}

static int wait_update_complete(void)
{
    for(unsigned i=0;i<100000;i++)
        if(!(cmos_read(0x0a)&0x80))
            return 1;
    return 0;
}

static void snapshot(cmos_time_t*t)
{
    t->second=cmos_read(0x00);
    t->minute=cmos_read(0x02);
    t->hour=cmos_read(0x04);
    t->day=cmos_read(0x07);
    t->month=cmos_read(0x08);
    t->year=cmos_read(0x09);
    t->century=cmos_read(0x32);
}

static int same(const cmos_time_t*a,const cmos_time_t*b)
{
    return a->second==b->second&&a->minute==b->minute
        &&a->hour==b->hour&&a->day==b->day&&a->month==b->month
        &&a->year==b->year&&a->century==b->century;
}

static uint8_t from_bcd(uint8_t value)
{
    return (uint8_t)((value&0x0f)+10u*(value>>4));
}

int rtc_read(rtc_time_t*t)
{
    cmos_time_t a,b;
    if(!t||!wait_update_complete())
        return 0;
    snapshot(&a);
    for(unsigned tries=0;tries<8;tries++){
        if(!wait_update_complete())
            return 0;
        snapshot(&b);
        if(same(&a,&b))
            break;
        a=b;
        if(tries==7)
            return 0;
    }

    uint8_t status=cmos_read(0x0b);
    uint8_t pm=(uint8_t)(b.hour&0x80);
    b.hour&=0x7f;
    if(!(status&0x04)){
        b.second=from_bcd(b.second);
        b.minute=from_bcd(b.minute);
        b.hour=from_bcd(b.hour);
        b.day=from_bcd(b.day);
        b.month=from_bcd(b.month);
        b.year=from_bcd(b.year);
        b.century=from_bcd(b.century);
    }
    if(!(status&0x02)){
        if(pm&&b.hour<12)b.hour=(uint8_t)(b.hour+12);
        if(!pm&&b.hour==12)b.hour=0;
    }
    if(b.century<19||b.century>99)b.century=20;

    t->year=(uint16_t)((uint16_t)b.century*100u+b.year);
    t->month=b.month;t->day=b.day;t->hour=b.hour;
    t->minute=b.minute;t->second=b.second;
    return t->year>=2000&&t->year<=9999
        &&t->month>=1&&t->month<=12&&t->day>=1&&t->day<=31
        &&t->hour<=23&&t->minute<=59&&t->second<=59;
}
