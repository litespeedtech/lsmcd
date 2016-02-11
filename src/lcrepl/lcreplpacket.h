#ifndef _LCREPLPACKET_H
#define _LCREPLPACKET_H
enum
{
    RT_NOTFOUND         = 8, 
    RT_C2M_JOINREQ,
    RT_M2C_JOINACK,
    RT_SOCKS2C_OUTSYNC,
    RT_M2C_MSTRFLIPING,//m->c
    RT_C2M_FLIPINGACK,   //c->m
    RT_M2C_FLIPDONE,  //m->c
    RT_GROUP_NEWMASTER     //m->group
};

#endif // REPLPACKET_H
