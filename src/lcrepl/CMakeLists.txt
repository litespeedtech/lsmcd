    SET(lcrepl_STAT_SRCS
    lcreplsender.cpp
    lcreplreceiver.cpp
    lcreplgroup.cpp
    replshmhelper.cpp
    lcshmevent.cpp
    lcnodeinfo.cpp
    lcreplconf.cpp
    config.cpp
    replicableshmtid.cpp
    replshmtidcontainer.cpp
    )
    add_library(lcrepl STATIC ${lcrepl_STAT_SRCS})
