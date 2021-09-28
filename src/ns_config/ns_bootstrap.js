/* This Javascript does the bootstraping for the  JS context */

/* example of debug print
__ns_js_debug_print("test debug string from javascript");
*/

function __ns_js_debug_print(str){
  if (__DEBUGGING__ == "ON")
    __ns_js_debug_print_callback(str);
}

function __ns_js_set_debug_flag_on(){
  __DEBUGGING__ = "ON";
};

function my_replace(str, find_str, repl_str)
{
  var newString = str.split(find_str);
  return newString.join(repl_str);
};


function __ns_js_empty() {
};



function __ns_js_node(id, value)
{
    /* Public members */
    this.id         = id;
    this.value      = value;
    this.innerHTML  = value;
};

function __ns_js_attribute(name)
{
    /* Public members */
    this.name       = name;
    this.value      = "";
    this.innerHTML  = this.value;
};

function __ns_js_element(tagName)
{
    /* Public members */
    this.tagName    = tagName;
    this.value      = "";
    this.innerHTML  = this.value;
};


function __ns_js_input_node(id, name, type, value, onclick)
{
    /* Public members */
    this.id         = id;
    this.name       = name;
    this.type       = type;
    this.value      = value;
    this.onclick    = onclick;
    this.innerHTML  = value;
};


function __ns_js_form(id, name, action, method, value)
{

    /* Private Members */

    /* Public members */
    this.id         = id;
    this.name       = name;
    this.action     = action;
    this.method     = method;
    this.value      = value;
    this.innerHTML  = value;

//    this.submit = __ns_js_empty;
};

function __ns_js_form_input(form_node_ptr, id, name, type, value, content, checked, multiple)
{
    this.id         = id;
    this.name       = name;
    this.type       = type;
    this.value      = value;
    this.content    = content;

    if(type != null && (type == "radio" || type == "checkbox" || type == "SELECT"))
      this.checked = checked;

    if(type != null && type == "SELECT")
      this.multiple = multiple;

    this.form_node_ptr  = form_node_ptr;
    if(content)
      this.innerHTML  = content;
    else 
      this.innerHTML  = value;
};


function __ns_js_documentelement(){
   return 1;
};

function __ns_js_domain () {
  return "www.example.com";
  //return 1;
};

function __ns_js_bgColor () {
  return 1;
};

function __ns_js_lastModified() {
  return 1;
};

function __ns_js_referrer() {
  return 1;
};

function __ns_js_title() {
  return 1;
};

/* DOM class */
function __ns_js_document() {

    /* Private members */
    this.__nodes  = new Array();
    this.__forms  = new Array();
    this.__inputs  = new Array();
    this.__attributes  = new Array();
    this.__elements  = new Array();
    this.forms_counter  = 0;
    this.inputs_counter  = 0;

    this.__write_buffer = "__CAV_NULL";

    /* Public members */
    this.__reset_write_buffer = function () {
        document.__write_buffer = "__CAV_NULL";
    }

    this.__append_to_write_buffer = function (str) {
      if(document.__write_buffer == "__CAV_NULL")
        document.__write_buffer = str;
      else
        document.__write_buffer += str;
      return 1;
    }
 
    this.__get_write_buffer = function () {
      ret = document.__write_buffer;
      this.__reset_write_buffer();
      return ret;
    }
   
    this.__add_node = function (id, value) {
        var elm = new __ns_js_node(id, value);
        document.__nodes[id] = elm;
    }

    this.__add_attribute = function (name) {
        var elm = new __ns_js_attribute(name);
        document.__attributes[name] = elm;
        return elm;
    }

    this.__add_element = function (tagName) {
        var elm = new __ns_js_element(tagName);
        document.__elements[tagName] = elm;
        return elm;
    }

    this.__set_attribute = function (name, value) {
        if (document.__attributes[name] != undefined && document.__attributes[name].value != undefined)
        document.__attributes[name].value = value;
        return 1;
    }

    this.__get_attribute = function (name) {
        if (document.__attributes[name] != undefined && document.__attributes[name].value != undefined)
        return document.__attributes[name].value;
    }

    this.__add_form = function (id, name, action, method, value) {
        var elm = new __ns_js_form(id, name, action, method, value);
        document.__forms[document.forms_counter] = elm;
        document.__forms[name] = elm;
        document.__forms[id] = elm;
        document.forms_counter++;
    }

    this.__add_input = function (form_node_ptr, id, name, type, value, content, checked, multiple) {

        var elm = new __ns_js_form_input(form_node_ptr, id, name, type, value, content, checked, multiple);
        document.__inputs[document.inputs_counter] = elm;
        document.__inputs[id] = elm;
        document.__inputs[name] = elm;
        document.inputs_counter++; 
    }

    this.__set_input = function (form_node_ptr, id, name, type, value, content, is_multiple) {

      __ns_js_debug_print("__set_input() method called; " + 
               "form_node_ptr=" + form_node_ptr + 
               ", id=" + id + 
               ", name=" + name +  
               ", type=" + type + 
               ", value=" + value +  
               ", content=" + content +
               ", is_multiple=" + is_multiple);

      var i,j;
 
      for (i=0; i<document.inputs_counter; i++)
      {
        if ((document.__inputs[i].id   == undefined || 
             document.__inputs[i].id   == null ||
             document.__inputs[i].id   == "") &&
 
            (document.__inputs[i].name == undefined || 
             document.__inputs[i].name == null ||
             document.__inputs[i].name == "") &&

            (document.__inputs[i].type == undefined || 
             document.__inputs[i].type == null ||
             document.__inputs[i].type == "")) 
          continue;
 
        if ((document.__inputs[i].id   != undefined) && (document.__inputs[i].id   != id  )) continue;
        if ((document.__inputs[i].name != undefined) && (document.__inputs[i].name != name)) continue;
        if ((document.__inputs[i].type != undefined) && (document.__inputs[i].type != type)) continue;

 
        if (form_node_ptr != null && 
            (document.__inputs[i].form_node_ptr == undefined ||
             document.__inputs[i].form_node_ptr == null || 
             document.__inputs[i].form_node_ptr != form_node_ptr ) )  
          continue;

        if(type == "SELECT" && document.__inputs[i].type == "SELECT")
        {
          /*  In case of multiple type select lists (where 'multiple' prop is defined)
           *  multiple values shall be a (,) separated string. Need to extract all
           *  values and set checked field for all of them as 1 
           *  If there is a comma in the value text, it will be encoded to %2C, so
           *  while saving the value, we need to uri decode it, as at the time of 
           *  creating the request query string (or post body), URI encoding is done.
           *  If it is not decoded here, it would be double encoded i.e. the % will
           *  also be encoded again, which is not ok.
           *  */

          if(/*value.indexOf(",") >= 0 ||*/ content.indexOf(",") >= 0)/* If there is a comma in  the string */
          {
            __ns_js_debug_print("Got comma in content, it means we have multiple options selected");

            /* Remove any white spaces */
//            value = value.replace(/^\s+|\s+$/g, '');
//            __ns_js_debug_print("after trimming new value=" + value);
            content = content.replace(/^\s+|\s+$/g, '');
            __ns_js_debug_print("after trimming new content=" + content);
 
            while(1)
            {
              /* This is the case of multiple selected options 
               * If the multiple is disabled, it should take only 1st content. */

              /* take first option value and content */
//              var local_value = value.slice(0, value.indexOf(","));
//              __ns_js_debug_print("local_value=" + local_value);
              var local_content = content.slice(0, content.indexOf(","));
              __ns_js_debug_print("local_content=" + local_content);

              /* Trim white spaces if any */
//              local_value = local_value.replace(/^\s+|\s+$/g, '');
//              __ns_js_debug_print("Trimmed local_value=" + local_value);
              local_content = local_content.replace(/^\s+|\s+$/g, '');
              __ns_js_debug_print("Trimmed local_content=" + local_content);

              /* Add first content; should not uri decode here, because if the content
               * has a comma, it will again try to slice the content string, and then 
               * individual content will mismatch with the content set at the time
               * of feeding form input elements at the time of JS bootstrap
               * */
//              this.__set_input(form_node_ptr, id, name, type, local_value, local_content, 1);
              this.__set_input(form_node_ptr, id, name, type, 0, local_content, 1);

              /* If this list is not multiple return after setting first value */
              if(document.__inputs[i].multiple != 1){ 
              
                __ns_js_debug_print("multiple is disabled for this select list. returning after setting first content");
                return 1;
              }

              /* remove first content from list */ 
//              value = value.slice(value.indexOf(",")+1);
//              __ns_js_debug_print("new value=" + value);
              content = content.slice(content.indexOf(",")+1);
              __ns_js_debug_print("new content=" + content);

              /* Remove any white spaces */
//              value = value.replace(/^\s+|\s+$/g, '');
//              __ns_js_debug_print("after trimming new value=" + value);
              content = content.replace(/^\s+|\s+$/g, '');
              __ns_js_debug_print("after trimming new content=" + content);

              /* If this is the last one, set it and return */
              if(/*(value && value.indexOf(",") < 0) ||*/ (content && content.indexOf(",") < 0))
              {
//                this.__set_input(form_node_ptr, id, name, type, value, content, 1);
                this.__set_input(form_node_ptr, id, name, type, 0, content, 1);
                return 1;
              }
            }
          }

          if (/*(document.__inputs[i].value != undefined &&
               document.__inputs[i].value != null &&
               document.__inputs[i].value != "" &&
               document.__inputs[i].value == value) ||*/
              (document.__inputs[i].content != undefined &&
               document.__inputs[i].content != null &&
               document.__inputs[i].content != "" &&
               document.__inputs[i].content == my_replace(content, "%2C", ",")))
          {

//            __ns_js_debug_print("select option value or content matched; value=" + value + 
            __ns_js_debug_print("select option value or content matched, content=" + content);
            for(j=0; j<document.inputs_counter; j++)
            {
              if(document.__inputs[j].type == "SELECT" && 
                 document.__inputs[j].form_node_ptr == document.__inputs[i].form_node_ptr &&
                 document.__inputs[j].name == document.__inputs[i].name){
 
                if(document.__inputs[j].checked != 2)/*2 means multiple options selected */
                  document.__inputs[j].checked = 0;
              }
            }

            if(is_multiple == 1)
            {
              __ns_js_debug_print("setting checked = 2");
              document.__inputs[i].checked = 2;
            }else{
              __ns_js_debug_print("setting checked = 1");
              document.__inputs[i].checked = 1;
            }
            break;
          } else continue; 
        }
        
        if(type == "radio" && document.__inputs[i].type == "radio" &&  document.__inputs[i].name == name)
        {
          for(j=i; j<document.inputs_counter; j++){
//            if(document.__inputs[j].type == "radio" && document.__inputs[j].name == name)
            if(document.__inputs[j].name != name ||
               document.__inputs[j].id != id ||
               document.__inputs[j].form_node_ptr != form_node_ptr ||
               document.__inputs[j].type != type)
              break
            if(document.__inputs[j].value != value)
              document.__inputs[j].checked = 0;
            else 
              document.__inputs[j].checked = 1;
          }
          break;
        }

        if(type == "checkbox" && document.__inputs[i].type == "checkbox")
        {
         if(document.__inputs[i].checked == 1)
           document.__inputs[i].checked = 0;
         else 
           document.__inputs[i].checked = 1;
        }

        if (value) document.__inputs[i].value = value;
        if (content) document.__inputs[i].content = content;

        break;
 
      }
      __ns_js_debug_print(__ns_js_debug_print_all_input_elements_cglue());
      return 1;
    }



    /************** VISIBLE DOM *********** */

    this.cookie = "";
    this.documentElement = new __ns_js_documentelement();
    this.URL = window.location;
    this.location = this.URL;

    this.getElementById =  function (id){
      if (document.__nodes[id])
        return document.__nodes[id]
      else if (document.__forms[id])
        return document.__forms[id]
      else if (document.__inputs[id])
        return document.__inputs[id]
      else 
        return null;
    }
    

    this.write =  function (str){
//        return str;
//      document.__nodes["body"].innerHTML += str;
//
      document.__append_to_write_buffer(str);
    }

    this.addEventListener = function(){
      return 1;
    }
    
    this.attachEvent = function(){
      return 1;
    }

    this.createAttribute = function(name){
        return document.__add_attribute(name);
    }

    this.getAttribute = function(name){
        return document.__get_attribute(name);
    }


    this.createAttributeNS = function(namespaceURI, qualifiedName){
      return 1;
    }
    this.createCDATASection = function(data){
      return 1;
    }
    this.createComment = function(data){
      return 1;
    }
    this.createDocumentFragment = function(){
      return 1;
    }
    this.createElement = function(tagName){
      return document.__add_element(tagName);
    }
    this.createElementNS = function(namespaceURI, qualifiedName){
      return 1;
    }
    this.createEvent = function(eventType){
      return 1;
    }
    this.createExpression = function(xpathText, fun_ptr){
      return 1;
    }
    this.createProcessingInstruction = function(target, data){
      return 1;
    }
    this.createRange = function(){
      return 1;
    }
    this.createTextNode = function(data){
      return 1;
    }
    this.detachEvent = function(){
      return 1;
    }
    this.dspatchEvent = function(){
      return 1;
    }
    this.evaluate = function(){
      return 1;
    }
    this.getElementsByTagName =  function (tag){
      return 1;
    }
    this.getElementsByTagNameNS =  function (namespceURI, localname){
      return 1;
    }
    this.importNode =  function (importedNode, deep){
      return 1;
    }
    this.loadXML =  function (text){
      return 1;
    }
    this.removeEventListener =  function (id){
      return 1;
    }
    //this.domain = new __ns_js_domain ();
    this.domain = "www.example.com";
    this.bgColor = new __ns_js_bgColor();
    this.lastModified = new __ns_js_lastModified();
    this.referrer = new __ns_js_referrer();
    this.title = new __ns_js_title();
};

function __ns_js_navigator(){
  this.appCodeName = "Mozilla";
  this.appName = "Mozilla";
  this.appVersion = "5.0 (Windows; en-GB)";
  this.cookieEnabled = "true";
  this.platform = "Win32";
  this.userAgent = "Mozilla/5.0 (Windows; U; Windows NT 6.1; en-GB; rv:1.9.2.17) Gecko/20110420 AskTbPTV/3.11.3.15590 Firefox/3.6.17";
  this.javaEnabled = function (){
    return true;
  }
  this.taintEnabled = function (){
    return true;
  }
};

function __ns_js_screen(){
  this.availHeight = 576;
  this.availWidth = 1093;
  this.colorDepth = 24;
  this.height = 1093;
  this.pixelDepth = 24;
  this.width = 614;
};

function __ns_js_defaultStatus(){
  return 1;
};

function __ns_js_history(){
  this.go =  function(value){
    return 1;
  }
  this.back =  function(){
    return 1;
  }
  this.forward =  function(){
    return 1;
  }
  this.length = "2";
};


function __ns_js_location(){
   this.hash = "This is location's hash";
   this.host = "www.default.com:80";
   this.hostname = "www.default.com";
   this.href = "http://www.example.com";
   this.pathname = "/this/is/default/path";
   this.port = "80";
   this.protocol = "http:";
   this.search = "?q=Javascript";
   this.reload = function(force){
     return 1;
   }
   this.replace = function(url){
     return 1;
   }
};

function __ns_js_name(){
  return  1;
};

function __ns_js_opener(){
  return  1;
};

function __ns_js_closed(){
  return  1;
};

function __ns_js_parent() {
  return 1;
};

function __ns_js_pageXOffset() {
  return 1;
};

function __ns_js_pageYOffset() {
  return 1;
};

function __ns_js_top() {
  return 1;
};

function __ns_js_innerHeight(){
  return 1;
};

function __ns_js_innerWidth(){
  return 1;
};

function __ns_js_outerHeight(){
  return 1;
};

function __ns_js_outerWidth(){
  return 1;
};


/* Window Class */
function __ns_js_window() {
    this.onload = __ns_js_empty;
    this.navigator = new __ns_js_navigator();
    this.screen = new __ns_js_screen();


    /*Window methods*/
    this.alert = function (param){
      return 1;
    }
    this.blur = function (){
      return 1;
    }
    this.close = function (){
      return 1;
    }
    this.confirm = function (question){
      return 1;
    }
    this.focus = function (){
      return 1;
    }
    this.getComputedStyle = function (elt, str){
      return 1;
    }
    this.moveBy = function (dx, dy){
      return 1;
    }
    this.moveTo = function (x, y){
      return 1;
    }
    this.open = function (url, name, features, replace){
      return 1;
    }
    this.prompt = function (msg, dft_str){
      return 1;
    }
    this.resizeBy = function (dw, dh){
      return 1;
    }
    this.resizeTo = function (dw, dh){
      return 1;
    }
    this.scrollBy = function (dw, dh){
      return 1;
    }
    this.scrollTo = function (dw, dh){
      return 1;
    }
    this.setTimeout = function (fn, time) {
      return 1;
    }
    this.clearTimeout = function (id) {
      return 1;
    } 
    this.setInterval = function (fn, time) {
      return 1;
    }
    this.clearInterval = function (id) {
      return 1;
    } 
    
    /*Window Objects*/
    this.location = new __ns_js_location();
    this.name = new __ns_js_name();
    this.opener = new __ns_js_opener();
    this.closed = new __ns_js_closed();
    this.outerHeight = new __ns_js_outerHeight();
    this.outerWidth = new __ns_js_outerWidth();
    this.parent = new __ns_js_parent(); 
    this.pageXOffset = new __ns_js_pageXOffset();
    this.pageYOffset = new __ns_js_pageYOffset();
    this.top = new __ns_js_top();
    this.innerHeight = new __ns_js_innerHeight();
    this.innerWidth = new __ns_js_innerWidth();
    this.defaultStatus = new __ns_js_defaultStatus();
    this.history = new __ns_js_history();
};

function __ns_js_set_onload(onload_function) {
    window.onload = window[onload_function];
};

/* The Global environment */
var window = new __ns_js_window();
var document = new __ns_js_document();
var navigator = window.navigator;

var first = 1;/*used for query string*/
var __DEBUGGING__ = "OFF";

/*Window methods*/
var alert = window.alert;
var blur =  window.blur;
var close =  window.close;
var confirm = window.confirm;
var focus = window.focus;
var getComputedStyle = window.getComputedStyle;
var moveBy = window.moveBy;
var moveTo = window.moveTo;
var open = window.open;
var prompt = window.prompt;
var resizeBy = window.resizeBy;
var resizeTo = window.resizeTo;
var scrollBy = window.scrollBy;
var scrollTo = window.scrollTo;
var clearInterval = window.clearInterval;
var setTimeout = window.setTimeout;
var setInterval = window.setInterval;
var clearTimeout = window.clearTimeout;

/*Window Objects*/

var defaultStatus = window.defaultStatus;
var history = window.history;
var location = window.location;
var name = window.name;
var opener = window.opener;
var closed = window.closed;
var parent = window.parent;
var outerWidth = window.outerWidth;
var outerHeight = window.outerHeight;
var pageXOffset = window.pageXOffset;
var pageYOffset = window.pageYOffset;
var top = window.top;
var innerHeight = window.innerHeight;
var innerWidth = window.innerWidth;
var outerHeight = window.outerHeight;
var outerWidth = window.outerWidth;


//document.__add_node("body", "Hello");

/* These are invoked from C context, need global env to set up fisrt */

function __ns_js_set_user_agent_cglue(UA) {
    __ns_js_debug_print("__ns_js_set_user_agent_cglue(): method called; UA=" + UA);
    window.navigator.userAgent=UA;
    return 1;
}


function __ns_js_get_write_buffer_cglue() {
    return document.__get_write_buffer();
}
function __ns_js_add_element_cglue(id, value) {
    document.__add_node(id, value);
    return 1;
};

function __ns_js_add_form_element_cglue(id, name, action, method, value) {
    document.__add_form(id, name, action, method, value);
    return 1;
};

function __ns_js_add_form_input_element_cglue(form_node_ptr, id, name, type, value, content, checked, multiple) {
  document.__add_input(form_node_ptr, id, name, type, value, content, checked, multiple);
  return 1;
};

function __ns_js_get_window_location_cglue() {
    return window.location;
};

function __ns_js_get_window_location_href_cglue() {
    return (window.location.href);
};

function __ns_js_set_window_location_href_cglue(url) {
//    window.location = url;
    window.location.href = url;
    //window.location.href = "Testing location";
    return 1;
};

function __ns_js_get_form_data_cglue(form_node_ptr, form_encoding_type) 
{

  var form_data_string; 
  var select_index = -2;
  var cur_sel_name = "";

  __ns_js_debug_print("__ns_js_get_form_data_cglue: method called, form_node_ptr=" + 
                      form_node_ptr.toString(16) + ", form_encoding_type=" +
                      form_encoding_type);

  /* select_index:
   *  -2 means outside select options list
   *  -1 means inside select options list but a selected option is appended to q str
   *  >=0 means inside select list and contains the index of the first select option */

  if (document.inputs_counter >0)
  {
    for (i=0; i<document.inputs_counter; i++)
    {

      if((document.__inputs[i].form_node_ptr == undefined) ||
         (document.__inputs[i].form_node_ptr == null) ||
         (document.__inputs[i].form_node_ptr != form_node_ptr))
        continue;

      if(document.__inputs[i].name == undefined) continue;
      if(document.__inputs[i].name == null) continue; 
      if(document.__inputs[i].name == "") continue;

      if(document.__inputs[i].type == "SELECT" && cur_sel_name == document.__inputs[i].name)
      {
        if(document.__inputs[i].checked > 0)
          select_index = -1; // Now first element need not be saved

      } else {
        if(select_index>=0 && document.__inputs[select_index].checked == 0)
        /* append the name=value for default first select option
         * In case it's checked flag was set, it would have already been appended */
        {
          form_data_string = __append(select_index, form_data_string, form_encoding_type);
          select_index = -2; // Now outside the select list
        }

        if(document.__inputs[i].type == "SELECT" && select_index == -2){
          cur_sel_name = document.__inputs[i].name;
          select_index = i; // First element of next select list
        }

        if(document.__inputs[i].type != "SELECT")
          select_index = -2; //outside select option list
        
      }
      

      if((document.__inputs[i].value == undefined ||
         document.__inputs[i].value == null ||
         document.__inputs[i].value == "") && 
         (document.__inputs[i].content == undefined ||
         document.__inputs[i].content == null ||
         document.__inputs[i].content == "") &&
         (document.__inputs[i].type == "checkbox" ||
         document.__inputs[i].type == "radio" ||
         document.__inputs[i].type == "image")) continue; 
      

      if((document.__inputs[i].type == "radio" || 
          document.__inputs[i].type == "checkbox" || 
          document.__inputs[i].type == "SELECT")&& 
         (document.__inputs[i].checked == undefined || 
          document.__inputs[i].checked == 0)) continue; 


      form_data_string = __append(i, form_data_string, form_encoding_type);
   }
  }
  return form_data_string;
};


function __append(i, form_data_string, form_encoding_type)
{
  var temp = "";

  if (first == 1){
    first =0;
    form_data_string = "";
  }
  else
  {
    switch(form_encoding_type)
    {
      case 1: /* text/plain */
        form_data_string = form_data_string + "\r\n";
        break;

      case 0: /* application/x-www-form-urlencoding */
      default:
        form_data_string = form_data_string + "&";
        break;
      /* TODO: case 2 i.e. multipart/form-data is yet to be supported, cuurently it is treated as case 0 */
    }
  }


  temp = document.__inputs[i].name;
  if(form_encoding_type != 1){
     /* not text/plain */
    temp = my_replace(temp, " ", "+");
    temp = escape(temp);
  }
  form_data_string = form_data_string + temp + "=";

  if(document.__inputs[i].value != undefined && 
     document.__inputs[i].value != null &&
     document.__inputs[i].value != "")
  {
  
    temp = document.__inputs[i].value;
    if(form_encoding_type != 1){
      /* not text/plain */
      temp = my_replace(temp, " ", "+");
      temp = escape(temp);
    }
    form_data_string = form_data_string + temp;
  }

  else if(document.__inputs[i].content != undefined && 
          document.__inputs[i].content != null &&
          document.__inputs[i].content != "")
  {
    temp = document.__inputs[i].content;
    if(form_encoding_type != 1){
       /* not text/plain */
      temp = my_replace(temp, " ", "+");
      temp = escape(temp);
    }
    form_data_string = form_data_string + temp;
  }

  return form_data_string;
}; 


function __ns_js_debug_print_all_input_elements_cglue()
{
  var dbg_buf; 

  if (document.inputs_counter >0)
  {
    dbg_buf = "\nINDEX|TYPE|FORM_NODE_PTR|ID|NAME|VALUE|CONTENT|CHECKED/SELECTED|MULTIPLE\n";
    dbg_buf +=  "==========================================================\n";
    for (i=0; i<document.inputs_counter; i++)
    {

      dbg_buf += i;
      dbg_buf += "|" + escape(document.__inputs[i].type); 
      dbg_buf += "|" + "0x" +document.__inputs[i].form_node_ptr.toString(16);/* prints hex value */ 
      dbg_buf += "|" + escape(document.__inputs[i].id); 
      dbg_buf += "|" + escape(document.__inputs[i].name); 
      dbg_buf += "|" + escape(document.__inputs[i].value);
      dbg_buf += "|" + escape(document.__inputs[i].content); 
      dbg_buf += "|" + document.__inputs[i].checked; /* Same is used as selected for selct options */
      dbg_buf += "|" + document.__inputs[i].multiple; 
      dbg_buf += "\n"; 
    }
  }else{

    dbg_buf = "\nNo Entry for INPUT elements\n";
  }

  return dbg_buf;

}

function __ns_js_set_input_element_cglue(form_node_ptr, id, name, type, value, content) 
{
  document.__set_input(form_node_ptr, id, name, type, value, content);
};

function __ns_js_get_element_cglue(id) {
    return document.getElementById(id).innerHTML;
};

function __ns_js_set_element_cglue(id, value) {
    document.getElementById(id).innerHTML = value;
    return 1;
};

function __ns_js_set_cookie(str) {
    document.cookie = str;
    return 1;
};

function __ns_js_emit_onload_event_cglue() {
    window.onload();
    return 1;
};

/* Global namespace browser functions */


