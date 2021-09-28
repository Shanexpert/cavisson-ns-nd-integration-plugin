
char *HdrStateArray[] = {
"HDST_BOL", "HDST_TEXT", "HDST_LF", "HDST_CR", "HDST_CRLF", "HDST_CRLFCR", "HDST_C", "HDST_CO", "HDST_CON", "HDST_CONT", "HDST_CONTE", "HDST_CONTEN", "HDST_CONTENT", "HDST_CONTENT_", "HDST_CONTENT_L", "HDST_CONTENT_LE", "HDST_CONTENT_LEN", "HDST_CONTENT_LENG", "HDST_CONTENT_LENGT", "HDST_CONTENT_LENGTH", "HDST_CONTENT_LENGTH_COLON", "HDST_CONTENT_LENGTH_COLON_VALUE", "HDST_CONTENT_E", "HDST_CONTENT_EN", "HDST_CONTENT_ENC", "HDST_CONTENT_ENCO", "HDST_CONTENT_ENCOD", "HDST_CONTENT_ENCODI", "HDST_CONTENT_ENCODIN", "HDST_CONTENT_ENCODING", "HDST_CONTENT_ENCODING_COLON", "HDST_CONTENT_ENCODING_COLON_G", "HDST_CONTENT_ENCODING_COLON_GZ", "HDST_CONTENT_ENCODING_COLON_GZI", "HDST_CONTENT_ENCODING_COLON_GZIP", "HDST_CONTENT_ENCODING_COLON_D", "HDST_CONTENT_ENCODING_COLON_DE", "HDST_CONTENT_ENCODING_COLON_DEF", "HDST_CONTENT_ENCODING_COLON_DEFL", "HDST_CONTENT_ENCODING_COLON_DEFLA", "HDST_CONTENT_ENCODING_COLON_DEFLAT", "HDST_CONTENT_ENCODING_COLON_DEFLATE", "HDST_CONTENT_ENCODING_COLON_B", "HDST_CONTENT_ENCODING_COLON_BR", "HDST_T", "HDST_TR", "HDST_TRA", "HDST_TRAN", "HDST_TRANS", "HDST_TRANSF", "HDST_TRANSFE", "HDST_TRANSFER", "HDST_TRANSFER_", "HDST_TRANSFER_E", "HDST_TRANSFER_EN", "HDST_TRANSFER_ENC", "HDST_TRANSFER_ENCO", "HDST_TRANSFER_ENCOD", "HDST_TRANSFER_ENCODI", "HDST_TRANSFER_ENCODIN", "HDST_TRANSFER_ENCODING", "HDST_TRANSFER_ENCODING_COLON", "HDST_TRANSFER_ENCODING_COLON_C", "HDST_TRANSFER_ENCODING_COLON_CH", "HDST_TRANSFER_ENCODING_COLON_CHU", "HDST_TRANSFER_ENCODING_COLON_CHUN", "HDST_TRANSFER_ENCODING_COLON_CHUNK", "HDST_TRANSFER_ENCODING_COLON_CHUNKE", "HDST_TRANSFER_ENCODING_COLON_CHUNKED", "HDST_L", "HDST_LO", "HDST_LOC", "HDST_LOCA", "HDST_LOCAT", "HDST_LOCATI", "HDST_LOCATIO", "HDST_LOCATION", "HDST_LOCATION_COLON", "HDST_LOCATION_COLON_VALUE", "HDST_S", "HDST_SE", "HDST_SET", "HDST_SET_", "HDST_SET_C", "HDST_SET_CO", "HDST_SET_COO", "HDST_SET_COOK", "HDST_SET_COOKI", "HDST_SET_COOKIE", "HDST_SET_COOKIE_COLON", "HDST_SET_COOKIE_COLON_VALUE", "HDST_CONTENT_T", "HDST_CONTENT_TY", "HDST_CONTENT_TYP", "HDST_CONTENT_TYPE", "HDST_CONTENT_TYPE_COLON", "HDST_CONTENT_TYPE_COLON_A", "HDST_CONTENT_TYPE_COLON_AP", "HDST_CONTENT_TYPE_COLON_APP", "HDST_CONTENT_TYPE_COLON_APPL", "HDST_CONTENT_TYPE_COLON_APPLI", "HDST_CONTENT_TYPE_COLON_APPLIC", "HDST_CONTENT_TYPE_COLON_APPLICA", "HDST_CONTENT_TYPE_COLON_APPLICAT", "HDST_CONTENT_TYPE_COLON_APPLICATI", "HDST_CONTENT_TYPE_COLON_APPLICATIO", "HDST_CONTENT_TYPE_COLON_APPLICATION", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_X", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_X_", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_X_A", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_X_AM", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_X_AMF", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_X_P", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_X_PR", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_X_PRO", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_X_PROT", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_X_PROTO", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_X_PROTOB", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_X_PROTOBU", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_X_PROTOBUF", "HDST_CONTENT_TYPE_COLON_X", "HDST_CONTENT_TYPE_COLON_X_", "HDST_CONTENT_TYPE_COLON_X_A", "HDST_CONTENT_TYPE_COLON_X_AP", "HDST_CONTENT_TYPE_COLON_X_APP", "HDST_CONTENT_TYPE_COLON_X_APPL", "HDST_CONTENT_TYPE_COLON_X_APPLI", "HDST_CONTENT_TYPE_COLON_X_APPLIC", "HDST_CONTENT_TYPE_COLON_X_APPLICA", "HDST_CONTENT_TYPE_COLON_X_APPLICAT", "HDST_CONTENT_TYPE_COLON_X_APPLICATI", "HDST_CONTENT_TYPE_COLON_X_APPLICATIO", "HDST_CONTENT_TYPE_COLON_X_APPLICATION", "HDST_CONTENT_TYPE_COLON_X_APPLICATION_SLASH_", "HDST_CONTENT_TYPE_COLON_X_APPLICATION_SLASH_P", "HDST_CONTENT_TYPE_COLON_X_APPLICATION_SLASH_PR", "HDST_CONTENT_TYPE_COLON_X_APPLICATION_SLASH_PRO", "HDST_CONTENT_TYPE_COLON_X_APPLICATION_SLASH_PROT", "HDST_CONTENT_TYPE_COLON_X_APPLICATION_SLASH_PROTO", "HDST_CONTENT_TYPE_COLON_X_APPLICATION_SLASH_PROTOB", "HDST_CONTENT_TYPE_COLON_X_APPLICATION_SLASH_PROTOBU", "HDST_CONTENT_TYPE_COLON_X_APPLICATION_SLASH_PROTOBUF", "HDST_CA", "HDST_CAC", "HDST_CACH", "HDST_CACHE", "HDST_CACHE_", "HDST_CACHE_C", "HDST_CACHE_CO", "HDST_CACHE_CON", "HDST_CACHE_CONT", "HDST_CACHE_CONTR", "HDST_CACHE_CONTRO", "HDST_CACHE_CONTROL", "HDST_CACHE_CONTROL_COLON", "HDST_CACHE_CONTROL_COLON_VALUE", "HDST_D", "HDST_DA", "HDST_DAT", "HDST_DATE", "HDST_DATE_COLON", "HDST_DATE_COLON_VALUE", "HDST_E", "HDST_EX", "HDST_EXP", "HDST_EXPI", "HDST_EXPIR", "HDST_EXPIRE", "HDST_EXPIRES", "HDST_EXPIRES_COLON", "HDST_EXPIRES_COLON_VALUE", "HDST_LA", "HDST_LAS", "HDST_LAST", "HDST_LAST_", "HDST_LAST_M", "HDST_LAST_MO", "HDST_LAST_MOD", "HDST_LAST_MODI", "HDST_LAST_MODIF", "HDST_LAST_MODIFI", "HDST_LAST_MODIFIE", "HDST_LAST_MODIFIED", "HDST_LAST_MODIFIED_COLON", "HDST_LAST_MODIFIED_COLON_VALUE", "HDST_ET", "HDST_ETA", "HDST_ETAG", "HDST_ETAG_COLON", "HDST_ETAG_COLON_VALUE", "HDST_A", "HDST_AG", "HDST_AGE", "HDST_AGE_COLON", "HDST_AGE_COLON_VALUE", "HDST_P", "HDST_PR", "HDST_PRA", "HDST_PRAG", "HDST_PRAGM", "HDST_PRAGMA", "HDST_PRAGMA_COLON", "HDST_PRAGMA_COLON_VALUE", "HDST_W", "HDST_WW", "HDST_WWW", "HDST_WWW_", "HDST_WWW_A", "HDST_WWW_AU", "HDST_WWW_AUT", "HDST_WWW_AUTH", "HDST_WWW_AUTHE", "HDST_WWW_AUTHEN", "HDST_WWW_AUTHENT", "HDST_WWW_AUTHENTI", "HDST_WWW_AUTHENTIC", "HDST_WWW_AUTHENTICA", "HDST_WWW_AUTHENTICAT", "HDST_WWW_AUTHENTICATE", "HDST_WWW_AUTHENTICATE_COLON", "HDST_WWW_AUTHENTICATE_COLON_VALUE", "HDST_PRO", "HDST_PROX", "HDST_PROXY", "HDST_PROXY_", "HDST_PROXY_A", "HDST_PROXY_AU", "HDST_PROXY_AUT", "HDST_PROXY_AUTH", "HDST_PROXY_AUTHE", "HDST_PROXY_AUTHEN", "HDST_PROXY_AUTHENT", "HDST_PROXY_AUTHENTI", "HDST_PROXY_AUTHENTIC", "HDST_PROXY_AUTHENTICA", "HDST_PROXY_AUTHENTICAT", "HDST_PROXY_AUTHENTICATE", "HDST_PROXY_AUTHENTICATE_COLON", "HDST_PROXY_AUTHENTICATE_COLON_VALUE", "HDST_CONTENT_TYPE_COLON_X_APPLICATION_SLASH_H", "HDST_CONTENT_TYPE_COLON_X_APPLICATION_SLASH_HE", "HDST_CONTENT_TYPE_COLON_X_APPLICATION_SLASH_HES", "HDST_CONTENT_TYPE_COLON_X_APPLICATION_SLASH_HESS", "HDST_CONTENT_TYPE_COLON_X_APPLICATION_SLASH_HESSI", "HDST_CONTENT_TYPE_COLON_X_APPLICATION_SLASH_HESSIA", "HDST_CONTENT_TYPE_COLON_X_APPLICATION_SLASH_HESSIAN", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_X_H", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_X_HE", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_X_HES", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_X_HESS", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_X_HESSI", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_X_HESSIA", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_X_HESSIAN", "HDST_X", "HDST_X_", "HDST_X_C", "HDST_X_CA", "HDST_X_CAC", "HDST_X_CACH", "HDST_X_CACHE", "HDST_X_CACHE_COLON", "HDST_X_CACHE_COLON_VALUE", "HDST_X_CACHE_", "HDST_X_CACHE_R", "HDST_X_CACHE_RE", "HDST_X_CACHE_REM", "HDST_X_CACHE_REMO", "HDST_X_CACHE_REMOT", "HDST_X_CACHE_REMOTE", "HDST_X_CACHE_REMOTE_COLON", "HDST_X_CACHE_REMOTE_COLON_VALUE", "HDST_X_CH", "HDST_X_CHE", "HDST_X_CHEC", "HDST_X_CHECK", "HDST_X_CHECK_", "HDST_X_CHECK_C", "HDST_X_CHECK_CA", "HDST_X_CHECK_CAC", "HDST_X_CHECK_CACH", "HDST_X_CHECK_CACHE", "HDST_X_CHECK_CACHEA", "HDST_X_CHECK_CACHEAB", "HDST_X_CHECK_CACHEABL", "HDST_X_CHECK_CACHEABLE", "HDST_X_CHECK_CACHEABLE_COLON", "HDST_X_CHECK_CACHEABLE_COLON_VALUE", "HDST_CF", "HDST_CF_", "HDST_CF_C", "HDST_CF_CA", "HDST_CF_CAC", "HDST_CF_CACH", "HDST_CF_CACHE", "HDST_CF_CACHE_", "HDST_CF_CACHE_S", "HDST_CF_CACHE_ST", "HDST_CF_CACHE_STA", "HDST_CF_CACHE_STAT", "HDST_CF_CACHE_STATU", "HDST_CF_CACHE_STATUS", "HDST_CF_CACHE_STATUS_COLON", "HDST_CF_CACHE_STATUS_COLON_VALUE", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_O", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_OC", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_OCT", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_OCTE", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_OCTET", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_OCTET_", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_OCTET_S", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_OCTET_ST", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_OCTET_STR", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_OCTET_STRE", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_OCTET_STREA", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_OCTET_STREAM", "HDST_CONN", "HDST_CONNE", "HDST_CONNEC", "HDST_CONNECT", "HDST_CONNECTI", "HDST_CONNECTIO", "HDST_CONNECTION", "HDST_CONNECTION_COLON", "HDST_CONNECTION_COLON_C", "HDST_CONNECTION_COLON_CL", "HDST_CONNECTION_COLON_CLO", "HDST_CONNECTION_COLON_CLOS", "HDST_CONNECTION_COLON_CLOSE", "HDST_LI", "HDST_LIN", "HDST_LINK", "HDST_LINK_COLON", "HDST_LINK_COLON_VALUE", "HDST_X_D", "HDST_X_DY", "HDST_X_DYN", "HDST_X_DYNA", "HDST_X_DYNAT", "HDST_X_DYNATR", "HDST_X_DYNATRA", "HDST_X_DYNATRAC", "HDST_X_DYNATRACE", "HDST_X_DYNATRACE_COLON", "HDST_X_DYNATRACE_COLON_VALUE", "HDST_U", "HDST_UP", "HDST_UPG", "HDST_UPGR", "HDST_UPGRA", "HDST_UPGRAD", "HDST_UPGRADE", "HDST_UPGRADE_COLON", "HDST_UPGRADE_COLON_VALUE", "HDST_SEC", "HDST_SEC_", "HDST_SEC_W", "HDST_SEC_WE", "HDST_SEC_WEB", "HDST_SEC_WEBS", "HDST_SEC_WEBSO", "HDST_SEC_WEBSOC", "HDST_SEC_WEBSOCK", "HDST_SEC_WEBSOCKE", "HDST_SEC_WEBSOCKET", "HDST_SEC_WEBSOCKET_", "HDST_SEC_WEBSOCKET_A", "HDST_SEC_WEBSOCKET_AC", "HDST_SEC_WEBSOCKET_ACC", "HDST_SEC_WEBSOCKET_ACCE", "HDST_SEC_WEBSOCKET_ACCEP", "HDST_SEC_WEBSOCKET_ACCEPT", "HDST_SEC_WEBSOCKET_ACCEPT_COLON", "HDST_SEC_WEBSOCKET_ACCEPT_COLON_VALUE", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_V", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_VN", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_VND", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_VND_DOT_", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_VND_DOT_A", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_VND_DOT_AP", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_VND_DOT_APP", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_VND_DOT_APPL", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_VND_DOT_APPLE", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_VND_DOT_APPLE_DOT_", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_VND_DOT_APPLE_DOT_M", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_VND_DOT_APPLE_DOT_MP", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_VND_DOT_APPLE_DOT_MPE", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_VND_DOT_APPLE_DOT_MPEG", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_VND_DOT_APPLE_DOT_MPEGU", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_VND_DOT_APPLE_DOT_MPEGUR", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_VND_DOT_APPLE_DOT_MPEGURL", "HDST_CONTENT_TYPE_COLON_V", "HDST_CONTENT_TYPE_COLON_VI", "HDST_CONTENT_TYPE_COLON_VID", "HDST_CONTENT_TYPE_COLON_VIDE", "HDST_CONTENT_TYPE_COLON_VIDEO", "HDST_CONTENT_TYPE_COLON_VIDEO_SLASH_", "HDST_CONTENT_TYPE_COLON_VIDEO_SLASH_M", "HDST_CONTENT_TYPE_COLON_VIDEO_SLASH_MP", "HDST_CONTENT_TYPE_COLON_VIDEO_SLASH_MP2", "HDST_CONTENT_TYPE_COLON_VIDEO_SLASH_MP2T", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_G", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_GR", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_GRP", "HDST_CONTENT_TYPE_COLON_APPLICATION_SLASH_GRPC", "HDST_G", "HDST_GR", "HDST_GRP", "HDST_GRPC", "HDST_GRPC_", "HDST_GRPC_E", "HDST_GRPC_EN", "HDST_GRPC_ENC", "HDST_GRPC_ENCO", "HDST_GRPC_ENCOD", "HDST_GRPC_ENCODI", "HDST_GRPC_ENCODIN", "HDST_GRPC_ENCODING", "HDST_GRPC_ENCODING_COLON", "HDST_GRPC_ENCODING_COLON_G", "HDST_GRPC_ENCODING_COLON_GZ", "HDST_GRPC_ENCODING_COLON_GZI", "HDST_GRPC_ENCODING_COLON_GZIP", "HDST_GRPC_ENCODING_COLON_D", "HDST_GRPC_ENCODING_COLON_DE", "HDST_GRPC_ENCODING_COLON_DEF", "HDST_GRPC_ENCODING_COLON_DEFL", "HDST_GRPC_ENCODING_COLON_DEFLA", "HDST_GRPC_ENCODING_COLON_DEFLAT", "HDST_GRPC_ENCODING_COLON_DEFLATE"
};
/* End of file */
