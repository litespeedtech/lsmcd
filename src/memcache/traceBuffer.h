
static void traceBuffer(const char *buf, int sz)
{
    int buf_len = (((sz + 15) / 16) * 16) * 2 + ((sz + 15) / 16) * (6 + 3 + 1 + 16 + 1) + 1;
    char buffer[buf_len];
    buffer[0] = 0;
    unsigned char *in = (unsigned char *)buf;
    int  buf_idx = 0;
    if (sz > 0xffff)
        sz = 0xffff;
    for (int i = 0; i < sz; i = i + 16)
    {
        snprintf(&buffer[buf_idx], buf_len - buf_idx, "%04x: ", i);
        buf_idx += 6;
        for (int j = i; (j < sz && j < i + 16); ++j)
        {
            snprintf(&buffer[buf_idx], buf_len - buf_idx, "%s%02x",
                     ((j == i) || (j % 4)) ? "" : " ", in[j]);
            buf_idx += (2 + (((j == i) || (j % 4)) ? 0 : 1));
        }
        if (i + 16 > sz)
        {
            for (int j = sz; j < sz + 16 - (sz % 16); ++j)
            {
                int incr = 2;
                if (j > sz && !(j % 4))
                    incr = 3;
                memcpy(&buffer[buf_idx], "   ", incr);
                buf_idx += incr;
            }
        }
        buffer[buf_idx] = ' ';
        buf_idx++;
        for (int j = i; (j < sz && j < i + 16); ++j)
        {
            if (in[j] >= ' ' && in[j] <= '~')
                buffer[buf_idx] = in[j];
            else
                buffer[buf_idx] = '.';
            buf_idx++;
        }
        buffer[buf_idx] = '\n';
        buf_idx++;
        buffer[buf_idx] = 0;
    }
#ifdef TRACE_PRINTF
    printf("Tracing %d bytes (allocated %d, used %d):\n%s", sz, buf_len,
            buf_idx, buffer);
#else
    LS_DBG_L("Tracing %d bytes (allocated %d, used %d):\n%s", sz, buf_len,
             buf_idx, buffer);
#endif
}

