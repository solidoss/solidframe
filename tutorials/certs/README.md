# SolidFrame - sample OpenSSL certificates

Make sure that
 * CA CN is: echo-ca
 * Server CN is: echo-server
 * Client CN is: echo-client


```bash
 $ openssl genrsa 2048 > echo-ca-key.pem
 $ openssl req -new -x509 -nodes -days 100000 -key echo-ca-key.pem > echo-ca-cert.pem
 $ openssl req -newkey rsa:2048 -days 100000 -nodes -keyout echo-server-key.pem > echo-server-req.pem
 $ openssl x509 -req -in echo-server-req.pem -days 100000 -CA echo-ca-cert.pem -CAkey echo-ca-key.pem -set_serial 01 > echo-server-cert.pem
 $ openssl req -newkey rsa:2048 -days 100000 -nodes -keyout echo-client-key.pem > echo-client-req.pem
 $ openssl x509 -req -in echo-client-req.pem -days 100000 -CA echo-ca-cert.pem -CAkey echo-ca-key.pem -set_serial 01 > echo-client-cert.pem
```
