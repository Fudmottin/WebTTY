openssl req \
  -x509 \
  -newkey rsa:2048 \
  -sha256 \
  -nodes \
  -days 30 \
  -keyout certs/server-key.pem \
  -out certs/server-cert.pem \
  -subj '/CN=localhost' \
  -addext 'subjectAltName=DNS:localhost,IP:127.0.0.1'

