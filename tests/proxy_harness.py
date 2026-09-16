"""Real HTTP CONNECT and SOCKS5 relays; credentials are test-only constants."""
import base64, http.server, socketserver, socket, select, threading, subprocess, sys, pathlib, urllib.parse
USER = 'test-user'
PASS = 'test @/:% password'
records = []
lock = threading.Lock()
def record(kind, host):
    with lock: records.append((kind, host))
def connect(host, port):
    if host == 'mediagrabby.test': host = '127.0.0.1'
    return socket.create_connection((host, port), timeout=20)
def relay(a, b):
    try:
        while True:
            ready, _, _ = select.select([a,b], [], [], 40)
            if not ready: break
            for src in ready:
                data = src.recv(65536)
                if not data: return
                (b if src is a else a).sendall(data)
    finally: b.close()
def readn(s, n):
    out = b''
    while len(out)<n:
        b=s.recv(n-len(out))
        if not b: raise ConnectionError('closed')
        out+=b
    return out
class HttpProxy(socketserver.BaseRequestHandler):
    def handle(self):
        self.request.settimeout(40)
        data=b''
        while b'\r\n\r\n' not in data: data+=readn(self.request,1)
        lines=data.decode('iso-8859-1').split('\r\n')
        headers={l.split(':',1)[0].lower():l.split(':',1)[1].strip() for l in lines[1:] if ':' in l}
        expected='Basic '+base64.b64encode((USER+':'+PASS).encode()).decode()
        if headers.get('proxy-authorization') != expected:
            self.request.sendall(b'HTTP/1.1 407 Proxy Authentication Required\r\nProxy-Authenticate: Basic realm="test"\r\nContent-Length: 0\r\nConnection: close\r\n\r\n');return
        method,target,version=lines[0].split(' ',2)
        if method=='CONNECT':
            host,port=target.rsplit(':',1);remote=connect(host,int(port));record('http',host)
            self.request.sendall(b'HTTP/1.1 200 Connection Established\r\n\r\n')
        else:
            u=urllib.parse.urlsplit(target);remote=connect(u.hostname,u.port or 80);record('http',u.hostname)
            path=urllib.parse.urlunsplit(('', '', u.path or '/',u.query,''))
            forwarded=[l for l in lines[1:] if l and not l.lower().startswith(('proxy-','connection:'))]
            remote.sendall((method+' '+path+' '+version+'\r\n'+'\r\n'.join(forwarded)+'\r\nConnection: close\r\n\r\n').encode('iso-8859-1'))
        relay(self.request,remote)
class SocksProxy(socketserver.BaseRequestHandler):
    def handle(self):
        s=self.request;s.settimeout(40)
        ver,n=readn(s,2);methods=readn(s,n)
        if ver!=5 or 2 not in methods: s.sendall(b'\x05\xff');return
        s.sendall(b'\x05\x02');ver,n=readn(s,2);user=readn(s,n);n=readn(s,1)[0];password=readn(s,n)
        if user!=USER.encode() or password!=PASS.encode():s.sendall(b'\x01\x01');return
        s.sendall(b'\x01\x00');ver,cmd,_,atyp=readn(s,4)
        if atyp==3:host=readn(s,readn(s,1)[0]).decode()
        elif atyp==1:host=socket.inet_ntoa(readn(s,4))
        elif atyp==4:host=socket.inet_ntop(socket.AF_INET6,readn(s,16))
        else:return
        port=int.from_bytes(readn(s,2),'big');remote=connect(host,port);record('socks5',host)
        s.sendall(b'\x05\x00\x00\x01\x00\x00\x00\x00\x00\x00');relay(s,remote)
class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address=True
    daemon_threads=True
    def handle_error(self,*args): pass
root=pathlib.Path(sys.argv[1]).resolve();root.mkdir(exist_ok=True)
class Files(http.server.SimpleHTTPRequestHandler):
    def __init__(self,*args,**kw):super().__init__(*args,directory=str(root),**kw)
    def log_message(self,*args):pass
servers=[Server(('127.0.0.1',18881),Files),Server(('127.0.0.1',18882),HttpProxy),Server(('127.0.0.1',18883),SocksProxy)]
for server in servers:threading.Thread(target=server.serve_forever,daemon=True).start()
try:
    result=subprocess.run([sys.argv[2],str(root),'18881',sys.argv[3],'proxy'],timeout=1200)
    if result.returncode:sys.exit(result.returncode)
    for kind in ('http','socks5'):
        assert (kind,'mediagrabby.test') in records, (kind,'video did not use proxy')
        assert any(k==kind and h=='nodejs.org' for k,h in records), (kind,'HTTPS did not use proxy')
    print('HTTP and SOCKS5 relays verified: video, HTTPS components, remote DNS, authentication')
finally:
    for server in servers:server.shutdown();server.server_close()
