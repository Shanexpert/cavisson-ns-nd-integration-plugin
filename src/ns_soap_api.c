#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/c14n.h>
#include <malloc.h>
#include <string.h>
#include "nslb_encode_decode.h"
#include "ns_soap_api.h"
/**************************************************************************************/
/*Internal Function*/
/**************************************************************************************/
/*Printf xml node*/
#if 0
static int ns_print_xml_element(xmlNodePtr node)
{
  xmlBufferPtr buf;
  buf = xmlBufferCreate();
  xmlNodeDump(buf,node->doc,node,0,1);
  printf("%s\n",(char*)xmlBufferContent(buf));
  xmlBufferFree(buf);
  return 0;
}

/*Print xml doc*/
static int ns_print_xml_doc(xmlDocPtr doc)
{
  xmlChar *buffer;
  int len;
  xmlDocDumpMemory(doc,&buffer,&len);
  printf("%s\n",buffer);
  xmlFree(buffer);
  return 0;
}
#endif
/*Copy xml node*/
static xmlNodePtr ns_copy_xml_element(xmlNodePtr node )
{
  xmlNsPtr *nsList;
  int i = 0;
  xmlNodePtr tmp = xmlCopyNode(node, 1);

  nsList = xmlGetNsList(tmp->doc, tmp);
  if (nsList != NULL) 
  {
    while (nsList[i] != NULL)
    {
      xmlNewNs(tmp, nsList[i]->href, nsList[i]->prefix);
      i++;
    }
  }
  xmlFree(nsList);

  return tmp;
}

/*Get string buffer of xml node*/
static xmlBufferPtr ns_get_xml_string(xmlNodePtr node )
{
  xmlBufferPtr buffer;
  xmlNsPtr *nsList;
  int i = 0;
  xmlNodePtr tmp = xmlCopyNode(node, 1);
 
  nsList = xmlGetNsList(tmp->doc, tmp);
  if (nsList != NULL) 
  {
    while (nsList[i] != NULL)
    {
      xmlNewNs(tmp, nsList[i]->href, nsList[i]->prefix);
      i++;
    }
  }
  xmlFree(nsList);
  buffer = xmlBufferCreate();
  xmlNodeDump(buffer,tmp->doc,tmp, 0, 0);
  xmlFreeNode(tmp);
 
  return buffer;
}

/*Get all childeren of a xml node*/
static xmlNodePtr soap_xml_get_children(xmlNodePtr param)
{
  xmlNodePtr children;

  if (param == NULL)
  {
    return NULL;
  }

  children = param->xmlChildrenNode;
  while (children != NULL)
  {
    if (children->type != XML_ELEMENT_NODE)
      children = children->next;
    else
      break;
  }

  return children;
}

/*Get next xml node*/
static xmlNodePtr soap_xml_get_next(xmlNodePtr param)
{
  xmlNodePtr node = param->next;

  while (node != NULL)
  {
    if (node->type != XML_ELEMENT_NODE)
      node = node->next;
    else
      break;
  }

  return node;
}

/*Get SOAP Body node in SOAP XML*/
static xmlNodePtr ns_get_soap_body(xmlDocPtr doc)
{
  xmlNodePtr root;
  xmlNodePtr node;

  root = xmlDocGetRootElement(doc);
  for (node = soap_xml_get_children(root); node; node = soap_xml_get_next(node))
  {
    if (!xmlStrcmp(node->name, BAD_CAST "Body"))
      return node;
  }

  return NULL;
}
/*Get SOAP Header node in SOAP XML*/
static xmlNodePtr ns_get_soap_header(xmlDocPtr doc)
{
  xmlNodePtr root;
  xmlNodePtr node;

  root = xmlDocGetRootElement(doc);
  for (node = soap_xml_get_children(root); node; node = soap_xml_get_next(node))
  {
    if (!xmlStrcmp(node->name, BAD_CAST "Header"))
      return node;
  }

  return NULL;
}

/*Insert WS Security node in header node*/
static xmlNodePtr ns_update_soap_header(xmlDocPtr doc, xmlNodePtr ws_security)
{
  xmlNodePtr child;
  xmlNodePtr header;

  header = ns_get_soap_header(doc);
  child = xmlFirstElementChild(header);
  xmlAddPrevSibling(child,ws_security);

  return header;
}

/*Get digest token xml node*/
static xmlNodePtr ns_get_xml_body_element(xmlDocPtr doc, char *element) 
{
  xmlNodePtr node;

  node = ns_get_soap_body(doc);
  while (node != NULL)
  {
    if (!xmlStrcmp(node->name, BAD_CAST element))
      break;
    node = soap_xml_get_next(node);
  }

  return node;
}

/*Transform xml into canonicalized form*/
static int ns_canonicalize_xml(xmlDocPtr doc, char *xml, char **canon_xml)
{
  xmlChar* c14_buf;
  xmlNodePtr new_root;
  int ret = 0;
  int with_comments = 0;
  xmlNsPtr *nsList = NULL;
  xmlDocPtr new_doc;
  int i = 0;
  int length;  
  xmlNodePtr root = NULL;

  xmlInitParser();
  LIBXML_TEST_VERSION
  
  length = strlen(xml);
  new_doc = xmlReadMemory(xml, length, "soapCanon.xml", NULL, 0);
  if (new_doc == NULL) {
    fprintf(stderr, "Error: unable to parse buffer\n");
    return -1;
  }
  new_root = xmlDocGetRootElement(new_doc);
  if(new_root == NULL) {
    fprintf(stderr,"Error: empty document\n");
    xmlFreeDoc(new_doc);
    xmlCleanupParser();
    return -1;
  }

  root = xmlDocGetRootElement(doc);
  nsList = xmlGetNsList(doc, root);
  if (nsList != NULL)
  {
    while (nsList[i] != NULL)
    {
      xmlNewNs(new_root,nsList[i]->href,nsList[i]->prefix);
      i++;
    }
  }
  xmlFree(nsList);
  ret = xmlC14NDocDumpMemory(new_doc, NULL, 0, NULL, with_comments, &c14_buf);
  if(ret < 0) {
    fprintf(stderr,"Error: failed to canonicalize XML\n");
    xmlFreeDoc(new_doc);
    xmlCleanupParser();
    return -1;
  }
  *canon_xml = (char *)c14_buf;
  xmlFreeDoc(new_doc);
  xmlCleanupParser();
  return 0;
}

/*Build Signed Info xml node*/
static xmlNodePtr ns_build_soap_signed_info(xmlDocPtr doc, char *digest_id, unsigned char *digest)
{
  int i = 0;
  char prefix[SOAP_SIG_BUF_SIZE] = "";
  char prefix2[SOAP_SIG_BUF_SIZE] = "";
  xmlNsPtr *nsList = NULL;
  xmlNodePtr root = NULL;
  xmlNodePtr signed_info = NULL;
  xmlNodePtr canon_method = NULL;
  xmlNodePtr inc_ns = NULL;
  xmlNodePtr inc_ns2 = NULL;
  xmlNodePtr sig_method = NULL;
  xmlNodePtr reference = NULL;
  xmlNodePtr transforms = NULL;
  xmlNodePtr transform = NULL;
  xmlNodePtr digest_method = NULL;

  /*Init XML Parser*/
  //xmlInitParser();
  //LIBXML_TEST_VERSION

  root = xmlDocGetRootElement(doc);

  signed_info = xmlNewNode(NULL, BAD_CAST "ds:SignedInfo");
  canon_method = xmlNewChild(signed_info, NULL, BAD_CAST"ds:CanonicalizationMethod", NULL);
  xmlNewProp(canon_method, BAD_CAST"Algorithm", BAD_CAST"http://www.w3.org/2001/10/xml-exc-c14n#");
  inc_ns = xmlNewChild(canon_method, NULL, BAD_CAST"ec:InclusiveNamespaces", NULL); 
  nsList = xmlGetNsList(doc, root);
  if (nsList != NULL)
  {
    i = 0;
    prefix[0] = 0;
    prefix2[0] = 0;
    while (nsList[i] != NULL)
    {
      if(!prefix[0])
        sprintf(prefix, "%s", nsList[i]->prefix);
      else 
        sprintf(prefix, "%s %s", prefix, nsList[i]->prefix);

      if(strcmp((char*)nsList[i]->href, "http://schemas.xmlsoap.org/soap/envelope/"))
      {
        if(!prefix2[0])
          sprintf(prefix2, "%s", nsList[i]->prefix);
        else
          sprintf(prefix2, "%s %s", prefix2, nsList[i]->prefix);
      }
      i++;
    }
  } 
  xmlFree (nsList);

  xmlNewProp(inc_ns, BAD_CAST"PrefixList", BAD_CAST(prefix));
  xmlNewNs(inc_ns, BAD_CAST"http://www.w3.org/2001/10/xml-exc-c14n#", BAD_CAST"ec");

  sig_method = xmlNewChild(signed_info, NULL, BAD_CAST"ds:SignatureMethod", NULL);
  xmlNewProp(sig_method, BAD_CAST"Algorithm", BAD_CAST"http://www.w3.org/2000/09/xmldsig#rsa-sha1");

  reference = xmlNewChild(signed_info, NULL, BAD_CAST"ds:Reference", NULL);
  sprintf(prefix, "#%s", digest_id);
  xmlNewProp(reference, BAD_CAST"URI", BAD_CAST(prefix));

  transforms = xmlNewChild(reference, NULL, BAD_CAST"ds:Transforms", NULL);
  transform = xmlNewChild(transforms, NULL, BAD_CAST"ds:Transform", NULL);
  xmlNewProp(transform, BAD_CAST"Algorithm", BAD_CAST"http://www.w3.org/2001/10/xml-exc-c14n#");

  inc_ns2 = xmlNewChild(transform, NULL, BAD_CAST"ec:InclusiveNamespaces", NULL);
  xmlNewProp(inc_ns2, BAD_CAST"PrefixList", BAD_CAST(prefix2));
  xmlNewNs(inc_ns2, BAD_CAST"http://www.w3.org/2001/10/xml-exc-c14n#", BAD_CAST"ec");

  digest_method = xmlNewChild(reference, NULL, BAD_CAST"ds:DigestMethod", NULL);
  xmlNewProp(digest_method, BAD_CAST"Algorithm", BAD_CAST"http://www.w3.org/2000/09/xmldsig#sha1"); 

  xmlNewChild(reference, NULL, BAD_CAST"ds:DigestValue", BAD_CAST(digest));

  return signed_info;
}

/*Build WS Security xml node*/
static xmlNodePtr ns_build_soap_wsse_security(nsSoapWSSecurityInfo *ns_ws_info, xmlDocPtr doc , unsigned char *hash)
{
  xmlNodePtr wsse_node;
  xmlNodePtr signed_info;
  xmlNodePtr node;
  char buf[SOAP_VAR_BUF_SIZE+1] = "";
  unsigned char signature[SOAP_SIG_BUF_SIZE] = "";
  char *canonical_xml;
  xmlBufferPtr xml_buf;
  wsse_node = xmlNewNode(NULL, BAD_CAST"wsse:Security");  
  xmlNewNs(wsse_node, BAD_CAST"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd", BAD_CAST"wsse");
  xmlNewNs(wsse_node, BAD_CAST"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd", BAD_CAST"wsu");

  node = xmlNewChild(wsse_node, NULL, BAD_CAST"wsse:BinarySecurityToken", BAD_CAST(ns_ws_info->certificate));  
  xmlNewProp(node, BAD_CAST"EncodingType", BAD_CAST"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary");
  xmlNewProp(node, BAD_CAST"ValueType", BAD_CAST"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-x509-token-profile-1.0#X509v3");
  xmlNewProp(node, BAD_CAST"wsu:Id", BAD_CAST(ns_ws_info->cert_id));

  node = xmlNewChild(wsse_node, NULL, BAD_CAST"ds:Signature",NULL);  
  xmlNewProp(node, BAD_CAST"Id", BAD_CAST(ns_ws_info->sign_id));
  xmlNewNs(node, BAD_CAST"http://www.w3.org/2000/09/xmldsig#", BAD_CAST"ds");
  signed_info = ns_build_soap_signed_info(doc, ns_ws_info->digest_id, hash); 
  xmlAddChild(node, signed_info);

  signed_info = ns_copy_xml_element(signed_info);
  xmlNewNs(signed_info, BAD_CAST"http://www.w3.org/2000/09/xmldsig#", BAD_CAST"ds");
  xml_buf = ns_get_xml_string(signed_info);
  ns_canonicalize_xml(doc,(char*)xmlBufferContent(xml_buf),&canonical_xml);
  xmlFreeNode(signed_info);
  xmlBufferFree(xml_buf);
  nslb_evp_sign(canonical_xml,strlen(canonical_xml),ns_ws_info->algorithm,ns_ws_info->key,signature, SOAP_SIG_BUF_SIZE);
  free(canonical_xml);

  xmlNewChild(node,NULL,BAD_CAST"ds:SignatureValue",BAD_CAST(signature));
  node = xmlNewChild(node,NULL,BAD_CAST"ds:KeyInfo",NULL);
  xmlNewProp(node, BAD_CAST"Id", BAD_CAST(ns_ws_info->key_info_id));
  node = xmlNewChild(node,NULL,BAD_CAST"wsse:SecurityTokenReference",NULL);
  xmlNewProp(node, BAD_CAST"wsu:Id", BAD_CAST(ns_ws_info->token_id));
  sprintf(buf,"#%s",ns_ws_info->cert_id);
  node = xmlNewChild(node,NULL,BAD_CAST"wsse:Reference",NULL);
  xmlNewProp(node, BAD_CAST"URI", BAD_CAST(buf));
  xmlNewProp(node,BAD_CAST"ValueType",BAD_CAST"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-x509-token-profile-1.0#X509v3");
  return wsse_node;
}

/*Start processing of input xml and generate outgoing xml*/
static int ns_proc_soap_xml(nsSoapWSSecurityInfo *ns_ws_info, char *xml , int xml_len , char **wsse_xml)
{
  unsigned char hash[65]="";
  char *canonical_xml;
  xmlBufferPtr xml_buf;
  xmlBufferPtr wsse_xml_buf;
  xmlDocPtr doc;
  xmlNodePtr root;
  xmlNodePtr element;
  xmlNodePtr ws_security;

  /*Init XML Parser*/
  xmlInitParser();
  LIBXML_TEST_VERSION

  doc = xmlReadMemory(xml,xml_len ,"soap.xml",NULL,0);
  if (doc == NULL) {
    fprintf(stderr, "Error: unable to parse buffer\n");
    return -1;
  }
  root = xmlDocGetRootElement(doc);
  if(root == NULL) {
    fprintf(stderr,"Error: empty document\n");
    xmlFreeDoc(doc);
    xmlCleanupParser();
    return -1;
  }
  /*Get digest XML element from soap body*/
  element = ns_get_xml_body_element(doc, ns_ws_info->token);
  if(!element)
  {
    fprintf(stderr,"Error: digest token not found\n");
    xmlFreeDoc(doc);
    xmlCleanupParser();
    return -1;
  }
  /*Insert digest id in digest element*/
  xmlNewProp(element,BAD_CAST"wsu:Id",BAD_CAST(ns_ws_info->digest_id));
  xmlNewNs(element,BAD_CAST"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd",BAD_CAST"wsu");

  /*convert into string */
  xml_buf = ns_get_xml_string(element);

  /*transform into canonical form*/
  ns_canonicalize_xml(doc,(char*)xmlBufferContent(xml_buf),&canonical_xml);

  /*Calculate Hash*/ 
  nslb_evp_digest(canonical_xml,strlen(canonical_xml),ns_ws_info->algorithm,hash);

  /*Generate WS Security Token*/
  ws_security = ns_build_soap_wsse_security(ns_ws_info,doc,hash);

  /*Insert WS Security Token into SOAP Header Token*/
  ns_update_soap_header(doc,ws_security);

  /*Convert XML DOC into string*/
  wsse_xml_buf = ns_get_xml_string(root);
  *wsse_xml = strdup((char*)xmlBufferContent(wsse_xml_buf));
  xmlBufferFree(xml_buf);
  xmlBufferFree(wsse_xml_buf);
  free(canonical_xml);

  xmlFreeDoc(doc);
  xmlCleanupParser();
  
  return 0;
}

/*Initialize ns_ws_info structure with default values*/
static int ns_init_soap_ws_security(nsSoapWSSecurityInfo *ns_ws_info)
{
  ns_ws_info->key = NULL;

  ns_ws_info->certificate = NULL;

  ns_ws_info->algorithm = DIGEST_SHA1;

  strncpy(ns_ws_info->token, "Body", SOAP_VAR_BUF_SIZE);
  ns_ws_info->token[SOAP_VAR_BUF_SIZE] = '\0';

  strncpy(ns_ws_info->digest_id, "id-1", SOAP_VAR_BUF_SIZE);
  ns_ws_info->digest_id[SOAP_VAR_BUF_SIZE] = '\0';

  strncpy(ns_ws_info->cert_id, "X509-CertificateId-1", SOAP_VAR_BUF_SIZE);
  ns_ws_info->cert_id[SOAP_VAR_BUF_SIZE] = '\0';

  strncpy(ns_ws_info->sign_id, "SIG-SignatureId-1", SOAP_VAR_BUF_SIZE);
  ns_ws_info->sign_id[SOAP_VAR_BUF_SIZE] = '\0';

  strncpy(ns_ws_info->key_info_id, "KI-KeyInfoId-1", SOAP_VAR_BUF_SIZE);
  ns_ws_info->key_info_id[SOAP_VAR_BUF_SIZE] = '\0';

  strncpy(ns_ws_info->token_id, "STR-SecurityTokenId-1", SOAP_VAR_BUF_SIZE);
  ns_ws_info->token_id[SOAP_VAR_BUF_SIZE] = '\0';

  return 0;
}
/*********************************************************************************/
/***************************API Function******************************************/
/*********************************************************************************/
/*Configure ns_ws_info structure*/
nsSoapWSSecurityInfo* ns_add_soap_ws_security(char *keyFile, char *certFile, int algorithm, char *token, char *digest_id, char *cert_id, char *sign_id, char *key_info_id, char* token_id)
{
  FILE *fp;
  char key_buf[SOAP_FILE_BUF_SIZE + 1];
  char cert_buf[SOAP_FILE_BUF_SIZE + 1];
  int key_len, cert_len;
  char *ptr1, *ptr2;
  nsSoapWSSecurityInfo *ns_ws_info;

  /*Validate Input*/ 
  if(!keyFile || !keyFile[0] || !certFile || !certFile[0])
  {
    fprintf(stderr, "Mandatory Arguments missing, Key File or Certificate File not provided\n");
    return NULL;
  }

  /*Read Key file*/
  fp = fopen(keyFile, "rb");
  if(fp == NULL)
  {
    fprintf(stderr, "Failed to open key file\n");
    return NULL;
  }
  key_len = fread(key_buf, 1, SOAP_FILE_BUF_SIZE, fp);
  fclose(fp);

  /*Read Certificate file*/
  fp = fopen(certFile, "rb");
  if(fp == NULL)
  {
    fprintf(stderr, "Failed to open certificate file\n");
    return NULL;
  }
  cert_len = fread(cert_buf, 1, SOAP_FILE_BUF_SIZE, fp);
  fclose(fp);

  ns_ws_info = (nsSoapWSSecurityInfo*)malloc(sizeof(nsSoapWSSecurityInfo));
  if(!ns_ws_info)
  {
    fprintf(stderr, "Failed to allocate memory for nsSoapWSSecurityInfo\n");
    return NULL;
  }

  /*Set Default Values*/
  ns_init_soap_ws_security(ns_ws_info);

  /*Set Key*/
  ns_ws_info->key = (char*)malloc(key_len + 1);
  if(!ns_ws_info->key)
  {
    fprintf(stderr, "Failed to alloate memory for key\n");
    ns_free_soap_ws_security(ns_ws_info);
    return NULL;
  }
  strncpy(ns_ws_info->key, key_buf, key_len);
  ns_ws_info->key[key_len] = '\0';   

  /*Set Certificate*/
  ptr1 = strstr(cert_buf, "-----BEGIN CERTIFICATE-----");
  if(ptr1)
  {
    //strlen("-----BEGIN CERTIFICATE-----") = 27
    ptr1 += 27;
    if(*ptr1 == '\r')
      ptr1++;

    if(*ptr1 == '\n')
      ptr1++;

    //ptr1 += 28 + 1; //strlen("-----BEGIN CERTIFICATE-----") = 28
    ptr2 = strstr(ptr1, "-----END CERTIFICATE-----");
    if(ptr2)
      cert_len = ptr2 - ptr1;
    else
    {
      fprintf(stderr, "Failed to read certificate file\n");
      //as we are not able to read cert file, hence calling ns_free_soap_ws_security for freeing memory malloc'd for ns_ws_info
      ns_free_soap_ws_security(ns_ws_info);
      return NULL;
    }
  }
  else
  {
    ptr1 = cert_buf;
  }
  ns_ws_info->certificate = (char*)malloc(cert_len + 1);  
  if(!ns_ws_info->certificate)
  {
    fprintf(stderr, "Failed to allocate memory for certificate\n");
    ns_free_soap_ws_security(ns_ws_info);
    return NULL;
  }

  strncpy(ns_ws_info->certificate, ptr1, cert_len);
  ns_ws_info->certificate[cert_len] = '\0';

  /*Set Algorithm*/
  if(algorithm)
    ns_ws_info->algorithm = algorithm;

  /*Set Digest Token*/ 
  if(token && token[0])
    strncpy(ns_ws_info->token, token, SOAP_VAR_BUF_SIZE);

  /*Set Digest Id*/ 
  if(digest_id && digest_id[0])
    strncpy(ns_ws_info->digest_id, digest_id, SOAP_VAR_BUF_SIZE);

  /*Set Certificate Id*/
  if(cert_id && cert_id[0])
    strncpy(ns_ws_info->cert_id, cert_id, SOAP_VAR_BUF_SIZE);

  /*Set Signature Id*/ 
  if(sign_id && sign_id[0])
    strncpy(ns_ws_info->sign_id, sign_id, SOAP_VAR_BUF_SIZE);

  /*Set Key Info Id*/
  if(key_info_id && key_info_id[0])
    strncpy(ns_ws_info->key_info_id, key_info_id, SOAP_VAR_BUF_SIZE);

  /*Set Security Id*/ 
  if(token_id && token_id[0])
    strncpy(ns_ws_info->token_id, token_id, SOAP_VAR_BUF_SIZE);

  return ns_ws_info;
}

int ns_update_soap_ws_security(nsSoapWSSecurityInfo *ns_ws_info, char *keyFile, char *certFile, int algorithm, char *token, char *digest_id, char *cert_id, char *sign_id, char *key_info_id, char* token_id)
{
  if(!ns_ws_info)
  {
    fprintf(stderr, "Failed to update WS Info as ns_ws_info not provided\n");
    return -1;
  }

  //update key & certificate
  if(keyFile && keyFile[0] && certFile && certFile[0])
  {
    FILE *fp;
    char key_buf[SOAP_FILE_BUF_SIZE + 1];
    char cert_buf[SOAP_FILE_BUF_SIZE + 1];
    int key_len, cert_len;
    char *ptr1, *ptr2;
    char *buf_ptr; 

    /*Read Key file*/
    fp = fopen(keyFile, "rb");
    if(fp == NULL)
    {
      fprintf(stderr, "Failed to open key file\n");
      return -1;
    }
    key_len = fread(key_buf, 1, SOAP_FILE_BUF_SIZE, fp);
    fclose(fp);

    /*Read Certificate file*/
    fp = fopen(certFile, "rb");
    if(fp == NULL)
    {
      fprintf(stderr, "Failed to open certificate file\n");
      return -1;
    }
    cert_len = fread(cert_buf, 1, SOAP_FILE_BUF_SIZE, fp);
    fclose(fp);

    /*Reallocate Key*/
    buf_ptr = (char*)realloc(ns_ws_info->key, key_len + 1);
    if(!buf_ptr)
    {
      fprintf(stderr, "Failed to alloate memory for key\n");
      return -1;

    }
    ns_ws_info->key = buf_ptr;    

    /*Reallocate Certificate*/
    buf_ptr = (char*)realloc(ns_ws_info->certificate, cert_len + 1);
    if(!buf_ptr)
    {
      fprintf(stderr, "Failed to alloate memory for certificate\n");
      return -1;
    }
    ns_ws_info->certificate = buf_ptr;

    /*Set Key*/
    strncpy(ns_ws_info->key, key_buf, key_len);
    ns_ws_info->key[key_len] = '\0';

    /*Set Certificate*/
    ptr1 = strstr(cert_buf, "-----BEGIN CERTIFICATE-----");
    if(ptr1)
    {
      //strlen("-----BEGIN CERTIFICATE-----") = 27
      ptr1 += 27;
      if(*ptr1 == '\r')
        ptr1++;
     
      if(*ptr1 == '\n')
        ptr1++;

      //ptr1 += 28 + 1; //strlen("-----BEGIN CERTIFICATE-----") = 28

      ptr2 = strstr(ptr1, "-----END CERTIFICATE-----");
      if(ptr2)
        cert_len = ptr2 - ptr1;
      else
      {
        fprintf(stderr, "Failed to read certificate file\n");
        return -1;
      }
    }
    else
    {
      ptr1 = cert_buf;
    }
    strncpy(ns_ws_info->certificate, ptr1, cert_len);
    ns_ws_info->certificate[cert_len] = '\0';
  }

  /*Set Algorithm*/
  if(algorithm)
    ns_ws_info->algorithm = algorithm;

  /*Set Digest Token*/ 
  if(token && token[0])
    strncpy(ns_ws_info->token, token, SOAP_VAR_BUF_SIZE);

  /*Set Digest Id*/ 
  if(digest_id && digest_id[0])
    strncpy(ns_ws_info->digest_id, digest_id, SOAP_VAR_BUF_SIZE);

  /*Set Certificate Id*/
  if(cert_id && cert_id[0])
    strncpy(ns_ws_info->cert_id, cert_id, SOAP_VAR_BUF_SIZE);

  /*Set Signature Id*/ 
  if(sign_id && sign_id[0])
    strncpy(ns_ws_info->sign_id, sign_id, SOAP_VAR_BUF_SIZE);

  /*Set Key Info Id*/
  if(key_info_id && key_info_id[0])
    strncpy(ns_ws_info->key_info_id, key_info_id, SOAP_VAR_BUF_SIZE);

  /*Set Security Id*/ 
  if(token_id && token_id[0])
    strncpy(ns_ws_info->token_id, token_id, SOAP_VAR_BUF_SIZE);

  return 0;
}


int ns_free_soap_ws_security(nsSoapWSSecurityInfo *ns_ws_info)
{
  if(!ns_ws_info)
    return -1;

  if(ns_ws_info->key)
    free(ns_ws_info->key);
  if(ns_ws_info->certificate)
    free(ns_ws_info->certificate);
  free(ns_ws_info);
  return 0;  
}
/*Generate WS Security XML*/
int ns_apply_soap_ws_security(nsSoapWSSecurityInfo *ns_ws_info , char *buffer, int buf_len , char **outbuf , int *outbuf_len)
{
  char *xml;
  char *start_xml,*end_xml ;
  char *wsse_xml;
  char *ptr1;
  char *ptr2;
  int xml_len;
  int wsse_xml_len;
  int filled = 0;
  int start_len; 
  int end_len;
  char *content_length_start;
  char *content_length_end;
  int content_length;
  char content_len_str[16];
  int content_len_str_len=0;
  int content_len_str_len_new=0;
  /*Validate Input*/ 
  if(!buffer || !outbuf)
  {
    fprintf(stderr, "Invalid input : buffer = %p , outbuf = %p\n", buffer, outbuf);
    return -1;
  }

  content_length_start = strstr(buffer, "Content-Length: ");
  if(content_length_start) 
  {
    content_length_start += 16;
    content_length_end = strstr(content_length_start, "\r\n");
    content_len_str_len = content_length_end - content_length_start;
    snprintf(content_len_str, content_len_str_len + 1, "%s", content_length_start);
    content_length = atoi(content_len_str);
  } 

  /*Read SOAP XML*/
  ptr1 = strstr(buffer, "xml version");
  if(ptr1)
  {
    ptr1 = strchr(ptr1,'<');
  }
  else
  {
    ptr1 = buffer;
  }
  ptr1 = strchr(ptr1,'<');
  if(!ptr1)
  {
    fprintf(stderr,"Invalid xml message\n");
    return -1;
  }
  ptr2 = strstr(ptr1,":Envelope");

  while(ptr1)
  {
    if(ptr1 < ptr2 )
    {
      start_xml = ptr1;
      ptr1++;
      ptr1 = strchr(ptr1,'<');
    }
    else
    {
      break;
    }
  }
  ptr1 = strstr(ptr1,":Envelope>");
  end_xml = ptr1 + 9;
  xml = start_xml;
  xml_len = end_xml - start_xml + 1;
  /*Process SOAP XML*/
  if(ns_proc_soap_xml(ns_ws_info, xml, xml_len, &wsse_xml) < 0)
  {
    fprintf(stderr, "Failed to process soap xml\n");
    return -1;
  }
  wsse_xml_len = strlen(wsse_xml);
  if (content_length_start)
  {
    content_length += wsse_xml_len - xml_len;   
    content_len_str_len_new = sprintf(content_len_str,"%d",content_length);
  }
  /*Generate in new buffer based on incoming buffer & soap ws security xml*/
  /*
  start_len = start_xml - buffer + (content_len_str_len_new - content_len_str_len);
  end_len = buf_len - (start_len + xml_len);
  *outbuf = (char*)malloc(start_len + wsse_xml_len + end_len + 1);
  */
  start_len = start_xml - buffer;
  end_len = buf_len - (start_len + xml_len) + 1;
  start_len += (content_len_str_len_new - content_len_str_len);
  *outbuf = (char*)malloc(start_len + wsse_xml_len + end_len + 1);
 
 // fprintf(stderr, "start_len = %d\nend_len = %d\nwsse_xml_len = %d\ncontent_len_str_len = %d\ncontent_len_str_len_new = %d\n", start_len, end_len, wsse_xml_len, content_len_str_len, content_len_str_len_new);
  if (*outbuf == NULL)
  {
    fprintf(stderr, "Failed to allocate memory\n");
    return -1;
  }

  if(start_len > 0)
  {
    if(content_length_start)
    {
      strncpy(&(*outbuf)[filled], buffer, (content_length_start - buffer));
      filled += content_length_start - buffer;
      strncpy(&(*outbuf)[filled], content_len_str, content_len_str_len_new);
      filled += content_len_str_len_new;
      strncpy(&(*outbuf)[filled], content_length_end, (start_xml - content_length_end));
      filled += start_xml - content_length_end;
  //    fprintf(stderr, "filled = %d\n", filled);
    }
    else
    {
      strncpy(&(*outbuf)[filled], buffer, start_len);
      filled += start_len;
    }
  }
  if(wsse_xml_len > 0)
  {
    strncpy(&(*outbuf)[filled],wsse_xml, wsse_xml_len);
    filled += wsse_xml_len;
  }
  if(end_len > 0)
  {
    strncpy(&(*outbuf)[filled], end_xml + 1, end_len);
    filled += end_len;
  }
  (*outbuf)[filled] = '\0';
  *outbuf_len = filled - 1 ;
  free(wsse_xml); 
  return 0;
}
/*****************************************************************************/
