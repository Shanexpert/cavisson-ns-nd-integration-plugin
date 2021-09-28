BUILD_MODE=non_debug
NS_BIN = netstorm
NS_DB_UPLOAD_BIN_NAME = nsu_db_upload
NS_STRING_API_SO_NAME = ns_string_api.so
ALL_LIBS = -L/home/himansh/git/cavisson/base/lib -lnstopo_v1 -lnscore -lnghttp2 -lmagic -lxml2_cav -lcprops -lprotobuf-c -lprotobuf -lprotoc -lmongoc-1.0 -lbson-1.0 -lcassandra -luv -lmqe_r -lmqic_r -ltibems64 -lrdkafka -lbrotlicommon -lbrotlienc -lbrotlidec -lssl -lcrypto -lntlm -lssh -ljs -lodbc -lldap -lcrypt -lnsc++ -L/usr/lib64 -L/usr/lib/x86_64-linux-gnu/ -lpthread -lgsl -lgslcblas -lm -lpq -ldl -lz -lm -lcurl -lrt -lgssapi_krb5 -lstdc++ -ldb -lduktape
PROD_LIBS = -L/home/himansh/git/cavisson/base/lib -lnstopo_v1 -lnscore -L/usr/lib64 -L/usr/lib/x86_64-linux-gnu/ -lm -lodbc -lxml2_cav
ND_AGGREGATOR_SO_NAME = nd_aggregator_api.so
NS_RELEASE = Ubuntu1604_64
