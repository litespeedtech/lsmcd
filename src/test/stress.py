#!/usr/bin/python3
print('Welcome to a python memcached (lsmcd) stress test.')
print('Find out when entries start getting purged')
print('Uses bmemcached (pip install python-binary-memcached)')
import bmemcached, argparse, random, sys, time
from bmemcached.exceptions import MemcachedException
parser = argparse.ArgumentParser(description='Stressing memcached (lsmcd) - all options require an argument (even if its 1 to enable)')
parser.add_argument("--u", default="user", type=str, help="SASL user name")
parser.add_argument("--p", default="password", type=str, help="SASL password")
parser.add_argument("--k", default=100, type=int, help="key size")
parser.add_argument("--v", default=10000, type=int, help="value size")
parser.add_argument("--c", default=100, type=int, help="how often to check to see that ALL of the key/values are there")
parser.add_argument("--n", default=0, type=int, help="set to 1 to turn off SASL")
parser.add_argument("--s", default=0, type=int, help="SASL user validation; key is constant, data has the user name in it")
parser.add_argument("--t", default=0, type=int, help="set to 1 to get stats, 2 to get stats only")
parser.add_argument("--g", default=0, type=int, help="get ONLY (assume set has been done)")
parser.add_argument("--o", default=1, type=int, help="just set key=value for this user (default is 1)")
parser.add_argument("--e", default=0, type=int, help="Number of entries to add/get before stopping - default is until an entry is missing")
parser.add_argument("--x", default=0, type=int, help="Test a specific size bit of random data - set and get - the parameter is the size")
args = parser.parse_args()
user = args.u
password = args.p
ks = args.k
vs = args.v
check = args.c
sasl_validate = args.s
maximum=args.x
if sasl_validate:
    print('Sasl validate')
get_stats = args.t
if get_stats:
    print('Getting stats')
get_only = args.g
if get_only:
    print('Getting only')
one_only = args.o
if one_only:
    print('Only key/value')
end_count = args.e
if maximum:
    one_only = 1
if sasl_validate:
    print('Doing sasl validate test')
print('Connect...')
server='127.0.0.1:11211'
if args.n == 1:
    print("Connecting without SASL")
    client = bmemcached.Client((server,),compression=None)
else:
    print("Connecting with SASL")
    client = bmemcached.Client((server,), user, password, compression=None)
if maximum > 0:
    repeated_value = int((time.time() * 1000000 % 255) + 1) 
    value = str(chr(repeated_value)) * maximum
    print("Binary character repeated " + str(maximum) + " times: " + str(repeated_value) + " length: " + str(len(value)))
else:
    value = 'value'
try:
    if (not get_only) and (not client.set('key', value)):
        print("Can't even save 1 simple key/value pair - fail")
        sys.exit()
except MemcachedException as meme:
    print('Memcached Exception: ' + str(meme) + ' in set')
    sys.exit()
    
try:    
    if get_stats == 2:
        stats = client.stats()
        statsValue = stats.get(server)
        for k, v in statsValue.items():
            print('   Stats[' + str(k) + '] = ' + v.decode())
        sys.exit()
    if not client.get('key'):
        if get_only:
            print("Unable to get single key - may not have been saved for this user")
        else:
            print("Can't even save and retrieve 1 simple key/value pair - fail")
        sys.exit()
except MemcachedException as meme:
    print('Memcached Exception: ' + str(meme) + ' in get')
    sys.exit()
    
#print('Getting key=' + client.get('key') + ' (should be value)')
if client.get('key') == value:
    print('Initial test works.')
else:
    print('Unexpected value of key: ' + client.get('key'))
if one_only:
    print('Completed test')
    sys.exit()
print('Now pound it!')
key_size = 0
data_size = 0
set_index = 0
error = False
key_format = '{0:0' + str(ks) + 'd}'
value_format = '{0:0>' + str(vs) + 'd}'
while True:
    set_index = set_index + 1
    key = key_format.format(set_index)
    if (sasl_validate):
        value = user + str(random.randrange(0, vs)).rjust(vs, '0')
    else:
        value = str(random.randrange(0, vs)).rjust(vs, '0')
    if not get_only and not client.set(key, value):
        print('Error in set of key #' + str(set_index) + ' after adding ' + str(key_size) + ' bytes of key and ' + str(data_size) + ' bytes of data')
        break
    key_size = key_size + ks
    data_size = data_size + vs
    if set_index % check == 0 or set_index == end_count:
        print('   Doing check at ' + str(set_index) + ' keys (added ' + str(key_size) + ' bytes of key and ' + str(data_size) + ' bytes of data)')
        if get_stats:
            stats = client.stats()
            statsValue = stats.get(server)
            for k, v in statsValue.items():
                print('   Stats[' + str(k) + '] = ' + v.decode())
        for get_index in range(1, set_index + 1):
            key = '{0:0100d}'.format(get_index)
            if (sasl_validate):
                value = user + '{0:010000d}'.format(get_index)
            else:
                value = '{0:010000d}'.format(get_index)
            try:
                server_value = client.get(key)
            except bmemcached.exceptions.MemcachedException as exc:
                print('Exception: {0}'.format(exc) + ' Error in get of key #' + str(get_index) + ' after adding ' + str(key_size) + ' bytes of key and ' + str(data_size) + ' bytes of data')
                error = True
                break
            except:
                print('Unknown Exception ' + ' Error in get of key #' + str(get_index) + ' after adding ' + str(key_size) + ' bytes of key and ' + str(data_size) + ' bytes of data')
                error = True
                break
            else:
                if (not server_value):
                    print('Error in get of key #' + str(get_index) + ' after adding ' + str(key_size) + ' bytes of key and ' + str(data_size) + ' bytes of data')
                    error = True
                    break
                if (not (server_value[0:len(user)] == user)) and sasl_validate:
                    print('Data leakage.  For key: ' + key + ' user: ' + server_value[0:len(user)] + ' expected: ' + user)
                    error = True
                    break
                if (not (len(server_value) == len(user) + vs)) and sasl_validate:
                    print('Bad length of server value: ' + str(len(server_value)) + ', expected: ' + str(len(user) + vs))
                    error = True
                    break;
        if set_index == end_count:
            break
    if error:
        break
print('Completed stress test')
    
