    SET(lcrepl_STAT_SRCS
    usockdata.cpp
    usockconn.cpp
    usockmcd.cpp
    usocklistener.cpp
    fdpasslstnr.cpp
    lcreplsender.cpp
    lcreplreceiver.cpp
    lcreplgroup.cpp
    replshmhelper.cpp
    lcnodeinfo.cpp
    lcreplconf.cpp
    config.cpp
    replicableshmtid.cpp
    replshmtidcontainer.cpp
    )
    add_library(lcrepl STATIC ${lcrepl_STAT_SRCS})
