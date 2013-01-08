/**
 * Author : Gérald FENOY
 *
 * Copyright (c) 2009-2012 GeoLabs SARL
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "service_internal.h"

#ifndef JSCLASS_GLOBAL_FLAGS
#define JSCLSAS_GLOBAL_FLAGS 0
#endif

static char dbg[1024];

JSBool
JSAlert(JSContext *cx, uintN argc, jsval *argv1)
{
  jsval *argv = JS_ARGV(cx,argv1);
  int i=0;
  JS_MaybeGC(cx);
  for(i=0;i<argc;i++){
    JSString* jsmsg = JS_ValueToString(cx,argv[i]);
    fprintf(stderr,"[ZOO-API:JS] %s\n",JS_EncodeString(cx,jsmsg));
  }
  JS_MaybeGC(cx);
  
  return JS_TRUE;
}

JSBool 
JSLoadScripts(JSContext *cx, uintN argc, jsval *argv1)
{
  //map* request = JS_GetContextPrivate(cx);
  //map* tmpm1=getMap(request,"metapath");
  JS_MaybeGC(cx);

  char ntmp[1024];
  getcwd(ntmp,1024);

  jsval *argv = JS_ARGV(cx,argv1);
  int i=0;
  JS_MaybeGC(cx);
  for(i=0;i<argc;i++){
    JSString* jsmsg = JS_ValueToString(cx,argv[i]);
    char *filename = JSValToChar(cx,&argv[i]);
    char *api0=(char*)malloc((strlen(ntmp)+strlen(filename)+2)*sizeof(char));
    sprintf(api0,"%s/%s",ntmp,filename);
#ifdef JS_DEBUG
    fprintf(stderr,"Trying to load %s\n",api0);
    fflush(stderr);
#endif
    JSObject *api_script1=loadZooApiFile(cx,JS_GetGlobalObject(cx),api0);
  }
  JS_MaybeGC(cx);
  JS_SET_RVAL(cx, argv1, JSVAL_VOID);
  
  return JS_TRUE;
}


int zoo_js_support(maps** main_conf,map* request,service* s,
		   maps **inputs,maps **outputs)
{
  maps* main=*main_conf;
  maps* _inputs=*inputs;
  maps* _outputs=*outputs;

  /* The class of the global object. */
  JSClass global_class= {
    "global", JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
  };

  /* JS variables. */
  JSRuntime *rt;
  JSContext *cx;
  JSObject  *global;

  /* Create a JS runtime. */
  rt = JS_NewRuntime(8L * 1024L * 1024L);
  if (rt == NULL)
    return 1;
  
  /* Create a context. */
  cx = JS_NewContext(rt,81920);
  if (cx == NULL){
    return 1;
  }
  JS_SetOptions(cx, JSOPTION_VAROBJFIX | JSOPTION_JIT | JSOPTION_METHODJIT);
  JS_SetVersion(cx, JSVERSION_LATEST);
  JS_SetErrorReporter(cx, reportError);

  /* Create the global object. */
  //#ifdef JS_NewCompartmentAndGlobalObject
  global = JS_NewCompartmentAndGlobalObject(cx, &global_class, NULL);
  //#else
  //global = JS_NewObject(cx, &global_class, 0,0);
  //#endif

  /* Populate the global object with the standard globals,
     like Object and Array. */
  if (!JS_InitStandardClasses(cx, global)){
    return 1;
  }

  jsval tmp=INT_TO_JSVAL(3);
  if (!JS_SetProperty(cx, global, "SERVICE_SUCCEEDED", &tmp))
    return 1;
  tmp=INT_TO_JSVAL(4);
  if (!JS_SetProperty(cx, global, "SERVICE_FAILED", &tmp))
    return 1;
  if (!JS_DefineFunction(cx, global, "ZOORequest", JSRequest, 4, 0))
    return 1;
  if (!JS_DefineFunction(cx, global, "ZOOTranslate", JSTranslate, 4, 0))
    return 1;
  if (!JS_DefineFunction(cx, global, "ZOOUpdateStatus", JSUpdateStatus, 2, 0))
    return 1;
  if (!JS_DefineFunction(cx, global, "alert", JSAlert, 2, 0))
    return 1;  
  if (!JS_DefineFunction(cx, global, "importScripts", JSLoadScripts, 1, 0))
    return 1;

  /**
   * Add private context object
   */
  void* cxPrivate = request;
  JS_SetContextPrivate(cx,cxPrivate);
    
  map* tmpm1=getMap(request,"metapath");
  char ntmp[1024];
  getcwd(ntmp,1024);

  /**
   * Load the first part of the ZOO-API
   */
  char *api0=(char*)malloc(strlen(tmpm1->value)+strlen(ntmp)+17);
  sprintf(api0,"%s/%s/ZOO-proj4js.js",ntmp,tmpm1->value);
#ifdef JS_DEBUG
  fprintf(stderr,"Trying to load %s\n",api0);
#endif
  JSObject *api_script1=loadZooApiFile(cx,global,api0);
  fflush(stderr);

  char *api1=(char*)malloc(strlen(tmpm1->value)+strlen(ntmp)+13);
  sprintf(api1,"%s/%s/ZOO-api.js",ntmp,tmpm1->value);
#ifdef JS_DEBUG
  fprintf(stderr,"Trying to load %s\n",api1);
#endif
  JSObject *api_script2=loadZooApiFile(cx,global,api1);
  fflush(stderr);

  /* Your application code here. This may include JSAPI calls
     to create your own custom JS objects and run scripts. */
  maps* out=*outputs;
  int res=SERVICE_FAILED;
  maps* mc=*main_conf;
  map* tmpm2=getMap(s->content,"serviceProvider");

  char *filename=(char*)malloc(strlen(tmpm1->value)+strlen(tmpm2->value)+strlen(ntmp)+3);
  sprintf(filename,"%s/%s/%s",ntmp,tmpm1->value,tmpm2->value);
  filename[strlen(tmpm1->value)+strlen(tmpm2->value)+strlen(ntmp)+2]=0;
#ifdef JS_DEBUG
  fprintf(stderr,"FILENAME %s\n",filename);
#endif
  struct stat file_status;
  stat(filename, &file_status);
  char *source=(char*)malloc(file_status.st_size);
  uint16 lineno;
  jsval rval;
  JSBool ok ;
  JSObject *script = JS_CompileFile(cx, global, filename);
  if(script!=NULL){
    (void)JS_ExecuteScript(cx, global, script, &rval);
  }
  else{
    char tmp1[1024];
    sprintf(tmp1,"Unable to load JavaScript file %s",filename);
    map* err=createMap("text",tmp1);
    addMapToMap(&err,createMap("code","NoApplicableCode"));
    printExceptionReportResponse(mc,err);
    JS_DestroyContext(cx);
    JS_DestroyRuntime(rt);
    JS_ShutDown();
    exit(-1);
  }

  /* Call a function in obj's scope. */
  jsval argv[3];
  JSObject *jsargv1=JSObject_FromMaps(cx,*main_conf);
  argv[0] = OBJECT_TO_JSVAL(jsargv1);
  JSObject *jsargv2=JSObject_FromMaps(cx,*inputs);
  argv[1] = OBJECT_TO_JSVAL(jsargv2);
  JSObject *jsargv3=JSObject_FromMaps(cx,*outputs);
  argv[2] = OBJECT_TO_JSVAL(jsargv3);
  jsval rval1=JSVAL_NULL;
#ifdef JS_DEBUG
  fprintf(stderr, "object %p\n", (void *) argv[2]);
#endif

  ok = JS_CallFunctionName(cx, global, s->name, 3, argv, &rval1);

#ifdef JS_DEBUG
  fprintf(stderr, "object %p\n", (void *) argv[2]);
#endif

  JSObject *d;
  if (ok==JS_TRUE && JSVAL_IS_OBJECT(rval1)==JS_TRUE) {
#ifdef JS_DEBUG
    fprintf(stderr,"Function run sucessfully !\n");
#endif
    /* Should get a number back from the service function call. */
    ok = JS_ValueToObject(cx, rval1, &d);
  }else{
    /* Unable to run JS function */
    char tmp1[1024];
    if(strlen(dbg)==0)
      sprintf(dbg,"No result was found after the function call");
    sprintf(tmp1,"Unable to run %s from the JavaScript file %s : \n %s",s->name,filename,dbg);
#ifdef JS_DEBUG
    fprintf(stderr,"%s",tmp1);
#endif
    map* err=createMap("text",tmp1);
    addToMap(err,"code","NoApplicableCode");
    printExceptionReportResponse(*main_conf,err);
    freeMap(&err);
    free(err);
    JS_DestroyContext(cx);
    JS_DestroyRuntime(rt);
    JS_ShutDown();
    // Should return -1 here but the unallocation won't work from zoo_service_loader.c line 1847
    exit(-1);
  }

  jsval t=OBJECT_TO_JSVAL(d);
  if(JS_IsArrayObject(cx,d)){
#ifdef JS_DEBUG
    fprintf(stderr,"An array was returned !\n");
#endif
    jsuint	 len;
    if((JS_GetArrayLength(cx, d, &len)==JS_FALSE)){
#ifdef JS_DEBUG
      fprintf(stderr,"outputs array is empty\n");
#endif
    }
    jsval tmp1;
    JSBool hasResult=JS_GetElement(cx,d,0,&tmp1);
    res=JSVAL_TO_INT(tmp1);
#ifdef JS_DEBUG
    fprintf(stderr," * %d * \n",res);
#endif
    if(res==SERVICE_SUCCEEDED){
      jsval tmp2;
      JSBool hasElement=JS_GetElement(cx,d,1,&tmp2);
      if(hasElement==JS_TRUE){
	freeMaps(outputs);
	free(*outputs);
	*outputs=mapsFromJSObject(cx,tmp2);
      }
    }else{
      jsval tmp3;
      JSBool hasConf=JS_GetElement(cx,d,1,&tmp3);
      if(hasConf==JS_TRUE){
	freeMaps(main_conf);
	free(*main_conf);
	*main_conf=mapsFromJSObject(cx,tmp3);
      }
    }

  }
  else{
#ifdef JS_DEBUG
    fprintf(stderr,"The service didn't return an array !\n");
#endif
    /**
     * Extract result
     */
    jsval tmp1;
    JSBool hasResult=JS_GetProperty(cx,d,"result",&tmp1);
    res=JSVAL_TO_INT(tmp1);

#ifdef JS_DEBUG
    fprintf(stderr," * %d * \n",res);
#endif
    /**
     * Extract outputs when available.
     */
    jsval tmp2;
    JSBool hasElement=JS_GetProperty(cx,d,"outputs",&tmp2);
    if(!JSVAL_IS_VOID(tmp2) && hasElement==JS_TRUE){
      freeMaps(outputs);
      free(*outputs);    
      *outputs=mapsFromJSObject(cx,tmp2);
    }
    JS_MaybeGC(cx);
#ifdef JS_DEBUG
    if(JSVAL_IS_VOID(tmp2))
      fprintf(stderr,"No outputs property returned\n");
    else{
      if(JS_IsArrayObject(cx,JSVAL_TO_OBJECT(tmp2)))
	fprintf(stderr,"outputs is an array as expected\n");
      else
	fprintf(stderr,"outputs is not an array as expected\n");
    }
    JS_MaybeGC(cx);
#endif

    /**
     * Extract conf when available.
     */
    jsval tmp3;
    JSBool hasConf=JS_GetProperty(cx,d,"conf",&tmp3);
    if(!JSVAL_IS_VOID(tmp3) && hasConf==JS_TRUE){
      freeMaps(main_conf);
      free(*main_conf);
      *main_conf=mapsFromJSObject(cx,tmp3);
    }
    JS_MaybeGC(cx);

#ifdef JS_DEBUG
    dumpMaps(*outputs);
#endif
  }
  /* Cleanup. */
  JS_MaybeGC(cx);
  JS_DestroyContext(cx);
  JS_DestroyRuntime(rt);
  JS_ShutDown();
#ifdef JS_DEBUG
  fprintf(stderr,"Returned value %d\n",res);
#endif
  return res;
}

JSObject * loadZooApiFile(JSContext *cx,JSObject  *global, char* filename){
  struct stat api_status;
  int s=stat(filename, &api_status);
  if(s==0){
    jsval rval;
    JSBool ok ;
    JSObject *script = JS_CompileFile(cx, JS_GetGlobalObject(cx), filename);
    if(script!=NULL){
      (void)JS_ExecuteScript(cx, JS_GetGlobalObject(cx), script, &rval);
#ifdef JS_DEBUG
      fprintf(stderr,"**************\n%s correctly loaded\n**************\n",filename);
#endif
      return script;
    }
#ifdef JS_DEBUG
    else
      fprintf(stderr,"\n**************\nUnable to run %s\n**************\n",filename);
#endif
  }
#ifdef JS_DEBUG
  else
    fprintf(stderr,"\n**************\nUnable to load %s\n**************\n",filename);
#endif
  return NULL;
}

JSObject* JSObject_FromMaps(JSContext *cx,maps* t){

  JSObject* res=JS_NewObject(cx, NULL, NULL, NULL);
  //JSObject *res = JS_NewArrayObject(cx, 0, NULL);
  if(res==NULL)
    fprintf(stderr,"Array Object is NULL!\n");
  maps* tmp=t;

  while(tmp!=NULL){
    jsuint len;
    JSObject* res1=JS_NewObject(cx, NULL, NULL, NULL);
    JSObject *pval=JSObject_FromMap(cx,tmp->content);
    jsval pvalj=OBJECT_TO_JSVAL(pval);
    JS_SetProperty(cx, res, tmp->name, &pvalj);

#ifdef JS_DEBUG
    fprintf(stderr,"Length of the Array %d, element : %s added \n",len,tmp->name);
#endif
    tmp=tmp->next;
  }  
  return res;
}

JSObject* JSObject_FromMap(JSContext *cx,map* t){
  JSObject* res=JS_NewObject(cx, NULL, NULL, NULL);
  jsval resf =  OBJECT_TO_JSVAL(res);
  map* tmpm=t;
  map* isArray=getMap(t,"isArray");
  map* isBinary=getMap(t,"size");
  map* tmap=getMapType(t);
#ifdef DEBUG
  if(tmap==NULL)
    fprintf(stderr,"tmap is null !\n");
  else
    fprintf(stderr,"tmap is not null ! (%s = %s)\n",tmap->name,tmap->value);
#endif
  /* Avoid gesture of binary content which failed due to strlen function use */
  if(isBinary!=NULL){
    return res;
  }
  while(tmpm!=NULL){
    if(isArray==NULL || strncasecmp(tmpm->name,"value",5)!=0 || 
       (tmap!=NULL && strncasecmp(tmpm->name,tmap->name,strlen(tmap->name))!=0)){
      jsval jsstr = STRING_TO_JSVAL(JS_NewStringCopyN(cx,tmpm->value,strlen(tmpm->value)));
      JS_SetProperty(cx, res, tmpm->name,&jsstr);
#ifdef JS_DEBUG
      fprintf(stderr,"%s => %s\n",tmpm->name,tmpm->value);
#endif
    }
    tmpm=tmpm->next;
  }
  if(isArray!=NULL){
    map* len=getMap(t,"length");
    int cnt=atoi(len->value);
    JSObject* values=JS_NewArrayObject( cx, cnt, NULL );
    JSObject* mvalues=JS_NewArrayObject( cx, cnt, NULL );
    map *tmpm1,*tmpm2;
    int i=0;
    for(i=0;i<cnt;i++){
      tmpm1=getMapArray(t,"value",i);
      tmpm2=getMapArray(t,tmap->name,i);
      if(tmpm1!=NULL){
	jsval jsstr = STRING_TO_JSVAL(JS_NewStringCopyN(cx,tmpm1->value,strlen(tmpm1->value)));
	JS_SetElement( cx, values, i, &jsstr );
      }
      if(tmpm2!=NULL){
	jsval jsstr = STRING_TO_JSVAL(JS_NewStringCopyN(cx,tmpm2->value,strlen(tmpm2->value)));
	JS_SetElement( cx, mvalues, i, &jsstr );
      }
    }
    jsval jvalues=OBJECT_TO_JSVAL(values);
    jsval jmvalues=OBJECT_TO_JSVAL(mvalues);
    JS_SetProperty(cx, res,"value",&jvalues);
    JS_SetProperty(cx, res,tmap->name,&jmvalues);
  }
  return res;
}

maps* mapsFromJSObject(JSContext *cx,jsval t){
  maps *res=NULL;
  maps *tres=NULL;
  jsint oi=0;
  JSObject* tt=JSVAL_TO_OBJECT(t);
  if(JS_IsArrayObject(cx,tt)){
#ifdef JS_DEBUG
    fprintf(stderr,"Is finally an array !\n");
#endif
  }
  else{
#ifdef JS_DEBUG
    fprintf(stderr,"Is not an array !\n");
#endif
    JSIdArray *idp=JS_Enumerate(cx,tt);
    if(idp!=NULL) {
      int index;
      jsdouble argNum;
#ifdef JS_DEBUG
      fprintf(stderr,"Properties length :  %d \n",idp->length);
#endif
      
      for (index=0,argNum=idp->length;index<argNum;index++) { 
	jsval id = idp->vector[index];
	jsval vp;
	JSString* str; 
	JS_IdToValue(cx,id,&vp);
	char *c, *tmp;
	JSString *jsmsg;
	size_t len1;
	jsmsg = JS_ValueToString(cx,vp);
	len1 = JS_GetStringLength(jsmsg);

	tres=(maps*)malloc(MAPS_SIZE);
	tres->name=strdup(JS_EncodeString(cx,jsmsg));
	tres->content=NULL;
	tres->next=NULL;

	jsval nvp=JSVAL_NULL;
	if((JS_GetProperty(cx, tt, JS_EncodeString(cx,jsmsg), &nvp)==JS_FALSE)){
#ifdef JS_DEBUG
	  fprintf(stderr,"Enumerate id : %d => %s => No more value\n",oi,JS_EncodeString(cx,jsmsg));
#endif
	}
	
	JSObject *nvp1=JSVAL_TO_OBJECT(JSVAL_NULL);
	JS_ValueToObject(cx,nvp,&nvp1);
	jsval nvp1j=OBJECT_TO_JSVAL(nvp1);
	if(JSVAL_IS_OBJECT(nvp1j)){
	  tres->content=mapFromJSObject(cx,nvp1j);
	}

	if(res==NULL)
	  res=dupMaps(&tres);
	else
	  addMapsToMaps(&res,tres);
	freeMaps(&tres);
	free(tres);
	tres=NULL;
	
	
      }
    }
  }

  jsuint len;
  JSBool hasLen=JS_GetArrayLength(cx, tt, &len);
#ifdef JS_DEBUG
  if(hasLen==JS_FALSE){
    fprintf(stderr,"outputs array is empty\n");
  }
  fprintf(stderr,"outputs array length : %d\n",len);
#endif
  for(oi=0;oi < len;oi++){
#ifdef JS_DEBUG
    fprintf(stderr,"outputs array length : %d step %d \n",len,oi);
#endif
    jsval tmp1;
    JSBool hasElement=JS_GetElement(cx,tt,oi,&tmp1);
    JSObject *otmp1=JSVAL_TO_OBJECT(tmp1);
    JSIdArray *idp=JS_Enumerate(cx,otmp1);
    if(idp!=NULL) {
      int index;
      jsdouble argNum;
#ifdef JS_DEBUG
      fprintf(stderr,"Properties length :  %d \n",idp->length);
#endif
      tres=(maps*)malloc(MAPS_SIZE);
      tres->name=NULL;
      tres->content=NULL;
      tres->next=NULL;

      for (index=0,argNum=idp->length;index<argNum;index++) { 
	jsval id = idp->vector[index];
	jsval vp;
	JSString* str; 
	JS_IdToValue(cx,id,&vp);
	char *c, *tmp;
	JSString *jsmsg;
	size_t len1;
	jsmsg = JS_ValueToString(cx,vp);
	len1 = JS_GetStringLength(jsmsg);
#ifdef JS_DEBUG
	fprintf(stderr,"Enumerate id : %d => %s\n",oi,JS_EncodeString(cx,jsmsg));
#endif
	jsval nvp=JSVAL_NULL;
	if((JS_GetProperty(cx, JSVAL_TO_OBJECT(tmp1), JS_EncodeString(cx,jsmsg), &nvp)==JS_FALSE)){
#ifdef JS_DEBUG
	  fprintf(stderr,"Enumerate id : %d => %s => No more value\n",oi,JS_EncodeString(cx,jsmsg));
#endif
	}
	
	if(JSVAL_IS_OBJECT(nvp)){
#ifdef JS_DEBUG
	  fprintf(stderr,"JSVAL NVP IS OBJECT\n");
#endif
	}

	JSObject *nvp1=JSVAL_TO_OBJECT(JSVAL_NULL);
	JS_ValueToObject(cx,nvp,&nvp1);
	jsval nvp1j=OBJECT_TO_JSVAL(nvp1);
	if(JSVAL_IS_OBJECT(nvp1j)){
	  JSString *jsmsg1;
	  JSObject *nvp2=JSVAL_TO_OBJECT(JSVAL_NULL);
	  jsmsg1 = JS_ValueToString(cx,nvp1j);
	  len1 = JS_GetStringLength(jsmsg1);
#ifdef JS_DEBUG
	  fprintf(stderr,"JSVAL NVP1J IS OBJECT %s = %s\n",JS_EncodeString(cx,jsmsg),JS_EncodeString(cx,jsmsg1));
#endif
	  if(strcasecmp(JS_EncodeString(cx,jsmsg1),"[object Object]")==0){
	    tres->name=strdup(JS_EncodeString(cx,jsmsg));
	    tres->content=mapFromJSObject(cx,nvp1j);
	  }
	  else
	    if(strcasecmp(JS_EncodeString(cx,jsmsg),"name")==0){
	      tres->name=strdup(JS_EncodeString(cx,jsmsg1));
	    }
	    else{
	      if(tres->content==NULL)
		tres->content=createMap(JS_EncodeString(cx,jsmsg),JS_EncodeString(cx,jsmsg1));
	      else
		addToMap(tres->content,JS_EncodeString(cx,jsmsg),JS_EncodeString(cx,jsmsg1));
	    }
	}
#ifdef JS_DEBUG
	else
	  fprintf(stderr,"JSVAL NVP1J IS NOT OBJECT !!\n");
#endif

      }
#ifdef JS_DEBUG
      dumpMaps(tres);
#endif
      if(res==NULL)
	res=dupMaps(&tres);
      else
	addMapsToMaps(&res,tres);
      freeMaps(&tres);
      free(tres);
      tres=NULL;

    }
  }
#ifdef JS_DEBUG
  dumpMaps(res);
#endif
  return res;
}

map* mapFromJSObject(JSContext *cx,jsval t){
  map *res=NULL;
  JSIdArray *idp=JS_Enumerate(cx,JSVAL_TO_OBJECT(t));
#ifdef JS_DEBUG
  fprintf(stderr,"Properties %p\n",(void*)t);
#endif
  if(idp!=NULL) {
    int index;
    jsdouble argNum;
#ifdef JS_DEBUG
    fprintf(stderr,"Properties length :  %d \n",idp->length);
#endif
    for (index=0,argNum=idp->length;index<argNum;index++) { 
      jsval id = idp->vector[index];
      jsval vp;
      JSString* str; 
      JS_IdToValue(cx,id,&vp);
      char *c, *tmp;
      JSString *jsmsg,*jsmsg1;
      size_t len,len1;
      jsmsg = JS_ValueToString(cx,vp);
      len = JS_GetStringLength(jsmsg);
      jsval nvp;
      JS_GetProperty(cx, JSVAL_TO_OBJECT(t), JS_EncodeString(cx,jsmsg), &nvp);
      jsmsg1 = JS_ValueToString(cx,nvp);
      len1 = JS_GetStringLength(jsmsg1);
#ifdef JS_DEBUG
      fprintf(stderr,"Enumerate id : %d [ %s => %s ]\n",index,JS_EncodeString(cx,jsmsg),JS_EncodeString(cx,jsmsg1));
#endif
      if(res!=NULL){
#ifdef JS_DEBUG
	fprintf(stderr,"%s - %s\n",JS_EncodeString(cx,jsmsg),JS_EncodeString(cx,jsmsg1));
#endif
	addToMap(res,JS_EncodeString(cx,jsmsg),JS_EncodeString(cx,jsmsg1));
      }
      else{
	res=createMap(JS_EncodeString(cx,jsmsg),JS_EncodeString(cx,jsmsg1));
	res->next=NULL;
      }
#ifdef JS_DEBUG
      dumpMap(res);
#endif
    }
  }
#ifdef JS_DEBUG
  dumpMap(res);
#endif
  return res;
}

/* The error reporter callback. */
void reportError(JSContext *cx, const char *message, JSErrorReport *report)
{
  sprintf(dbg,"%s:%u:%s\n",
	  report->filename ? report->filename : "<no filename>",
	  (unsigned int) report->lineno,
	  message);
#ifdef JS_DEBUG
  fprintf(stderr,"%s",dbg);
#endif
  fflush(stderr);
}

char* JSValToChar(JSContext* context, jsval* arg) {
  char *c;
  char *tmp;
  JSString *jsmsg;
  size_t len;
  int i;
  if(!JSVAL_IS_STRING(*arg)) {
    return NULL;
  }
  jsmsg = JS_ValueToString(context,*arg);
  len = JS_GetStringLength(jsmsg);
  tmp = JS_EncodeString(context,jsmsg);
  c = (char*)malloc((len+1)*sizeof(char));
  c[len] = '\0';
#ifdef ULINET_DEBUG
  fprintf(stderr,"%d \n",len);
#endif
  for(i = 0;i < len;i++) {
    c[i] = tmp[i];
    c[i+1] = 0;
  }
#ifdef ULINET_DEBUG
  fprintf(stderr,"%s \n",c);
#endif
  return c;
}

HINTERNET setHeader(HINTERNET handle,JSContext *cx,JSObject *header){
  jsuint length=0;
  jsint i=0;
  char *tmp1;
#ifdef ULINET_DEBUG
  fprintf(stderr,"setHeader\n");
#endif
  if(JS_IsArrayObject(cx,header)){
#ifdef ULINET_DEBUG
    fprintf(stderr,"header is an array\n");
#endif
    JS_GetArrayLength(cx,header,&length);
#ifdef ULINET_DEBUG
    fprintf(stderr,"header is an array of %d elements\n",length);
#endif
    handle.header=NULL;
    for(i=0;i<length;i++){
      jsval tmp;
      JS_GetElement(cx,header,i,&tmp);
      tmp1=JSValToChar(cx,&tmp);
#ifdef ULINET_DEBUG
      curl_easy_setopt(handle.handle,CURLOPT_VERBOSE,1);
      fprintf(stderr,"Element of array n° %d, value : %s\n",i,tmp1);
#endif
      handle.header=curl_slist_append(handle.header, tmp1);
      free(tmp1);
    }
  }
  else{
    fprintf(stderr,"not an array !!!!!!!\n");
  }
  return handle;
}

JSBool
JSTranslate(JSContext *cx, uintN argc, jsval *argv1)
{
  jsval *argv = JS_ARGV(cx,argv1);
  char *str=JSValToChar(cx,&argv[0]);
  char *tmpValue=_ss(str);
  JS_SET_RVAL(cx, argv1,STRING_TO_JSVAL(JS_NewStringCopyN(cx,tmpValue,strlen(tmpValue))));  
  JS_MaybeGC(cx);
  return JS_TRUE;
}

JSBool
JSRequest(JSContext *cx, uintN argc, jsval *argv1)
{
  jsval *argv = JS_ARGV(cx,argv1);
  HINTERNET hInternet;
  HINTERNET res;
  HINTERNET res1;
  JSObject *header;
  char *url;
  char *method;
  char* tmpValue;
  size_t dwRead;
  int i=0;
  JS_MaybeGC(cx);
  hInternet=InternetOpen("ZooWPSClient\0",
			 INTERNET_OPEN_TYPE_PRECONFIG,
			 NULL,NULL, 0);
  if(!CHECK_INET_HANDLE(hInternet))
    return JS_FALSE;
  if(argc>=2){
    method=JSValToChar(cx,&argv[0]);
    url=JSValToChar(cx,&argv[1]);
  }
  else{
    method=strdup("GET");
    url=JSValToChar(cx,argv);
  }
  if(argc==4){
    char *body;
    body=JSValToChar(cx,&argv[2]);
    header=JSVAL_TO_OBJECT(argv[3]);
#ifdef ULINET_DEBUG
    fprintf(stderr,"URL (%s) \nBODY (%s)\n",url,body);
#endif
    if(JS_IsArrayObject(cx,header))
      res1=setHeader(hInternet,cx,header);
#ifdef ULINET_DEBUG
    fprintf(stderr,"BODY (%s)\n",body);
#endif
    res=InternetOpenUrl(res1,url,body,strlen(body),
			INTERNET_FLAG_NO_CACHE_WRITE,0);    
    free(body);
  }else{
    if(argc==3){
      char *body=JSValToChar(cx,&argv[2]);
      res=InternetOpenUrl(hInternet,url,body,strlen(body),
			  INTERNET_FLAG_NO_CACHE_WRITE,0);
      free(body);
    }
    res=InternetOpenUrl(hInternet,url,NULL,0,
			INTERNET_FLAG_NO_CACHE_WRITE,0);
  }
  tmpValue=(char*)malloc((res.nDataLen+1)*sizeof(char));
  InternetReadFile(res,(LPVOID)tmpValue,res.nDataLen,&dwRead);
#ifdef ULINET_DEBUG
  fprintf(stderr,"content downloaded (%d) (%s) \n",dwRead,tmpValue);
#endif
  if(dwRead==0){
    JS_SET_RVAL(cx, argv1,STRING_TO_JSVAL(JS_NewStringCopyN(cx,"Unable to access the file.",strlen("Unable to access the file."))));
    return JS_TRUE;
  }

#ifdef ULINET_DEBUG
  fprintf(stderr,"content downloaded (%d) (%s) \n",dwRead,tmpValue);
#endif
  JS_SET_RVAL(cx, argv1,STRING_TO_JSVAL(JS_NewStringCopyN(cx,tmpValue,strlen(tmpValue))));
  free(url);
  if(argc>=2)
    free(method);
  if(argc==4 && res.header!=NULL){
    curl_slist_free_all(res.header);
  }
  InternetCloseHandle(hInternet);
  JS_MaybeGC(cx);
  return JS_TRUE;
}
