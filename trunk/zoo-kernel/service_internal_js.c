/**
 * Author : Gérald FENOY
 *
 * Copyright (c) 2009-2010 GeoLabs SARL
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

static char dbg[1024];

int zoo_js_support(maps** main_conf,map* request,service* s,
		   maps **inputs,maps **outputs)
{
  maps* main=*main_conf;
  maps* _inputs=*inputs;
  maps* _outputs=*outputs;

  /* The class of the global object. */
  JSClass global_class = {
    "global", JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
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
  cx = JS_NewContext(rt,8192);
  if (cx == NULL){
    return 1;
  }
  JS_SetOptions(cx, JSOPTION_VAROBJFIX);
  JS_SetVersion(cx, JSVERSION_LATEST);
  JS_SetErrorReporter(cx, reportError);

  /* Create the global object. */
  global = JS_NewObject(cx, &global_class, NULL, NULL);
  if (global == NULL){
    return 1;
  }

  /* Populate the global object with the standard globals,
     like Object and Array. */
  if (!JS_InitStandardClasses(cx, global)){
    return 1;
  }
  if (!JS_DefineFunction(cx, global, "ZOORequest", JSRequest, 4, 0))
    return 1;
  if (!JS_DefineFunction(cx, global, "ZOOUpdateStatus", JSUpdateStatus, 2, 0))
    return 1;

  /* Your application code here. This may include JSAPI calls
     to create your own custom JS objects and run scripts. */
  maps* out=*outputs;
  int res=SERVICE_FAILED;
  maps* mc=*main_conf;
  map* tmpm1=getMap(request,"metapath");
  map* tmpm2=getMap(s->content,"serviceProvider");
  char ntmp[1024];
  getcwd(ntmp,1024);
  char filename[strlen(tmpm1->value)+strlen(tmpm2->value)+strlen(ntmp)+2];
  sprintf(filename,"%s/%s%s",ntmp,tmpm1->value,tmpm2->value);
  filename[strlen(tmpm1->value)+strlen(tmpm2->value)+strlen(ntmp)+1]=0;
  fprintf(stderr,"FILENAME %s\n",filename);
  struct stat file_status;
  stat(filename, &file_status);
  char source[file_status.st_size];
  uint16 lineno;
  jsval rval;
  FILE *jsfile=fopen(filename,"r");
  JSBool ok ;
  JSScript *script = JS_CompileFileHandle(cx, global, filename,jsfile);
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
    fclose(jsfile);
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
    sprintf(tmp1,"Unable to run %s from the JavScript file %s : \n %s",s->name,filename,dbg);
    fprintf(stderr,"%s",tmp1);
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
    jsint len;
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
    jsval tmp2;
    JSBool hasElement=JS_GetElement(cx,d,1,&tmp2);
    if(hasElement==JS_TRUE){
      *outputs=mapsFromJSObject(cx,tmp2);
    }
  }
  else{
#ifdef JS_DEBUG
    fprintf(stderr,"The serice didn't return an array !\n");
#endif
    jsval tmp1;
    JSBool hasResult=JS_GetProperty(cx,d,"result",&tmp1);
    res=JSVAL_TO_INT(tmp1);

#ifdef JS_DEBUG
    fprintf(stderr," * %d * \n",res);
#endif
    jsval tmp2;
    JSBool hasElement=JS_GetProperty(cx,d,"outputs",&tmp2);
    if(!hasElement)
      fprintf(stderr,"No outputs property returned\n");
    if(JS_IsArrayObject(cx,JSVAL_TO_OBJECT(tmp2)))
      fprintf(stderr,"outputs is array an as expected\n");
    else
      fprintf(stderr,"outputs is not an array as expected\n");
    *outputs=mapsFromJSObject(cx,tmp2);
#ifdef JS_DEBUG
    dumpMaps(out);
#endif
  }

  /* Cleanup. */
  JS_DestroyScript(cx, script);
  JS_MaybeGC(cx);
  // If we use the DestroyContext as requested to release memory then we get 
  // issue getting back the main configuration maps after coming back to the 
  // runRequest function ...
  //JS_DestroyContext(cx);
  JS_DestroyRuntime(rt);
  JS_ShutDown();
#ifdef JS_DEBUG
  fprintf(stderr,"Returned value %d\n",res);
#endif
  return res;
}

JSObject* JSObject_FromMaps(JSContext *cx,maps* t){
  JSObject *res = JS_NewArrayObject(cx, 0, NULL);
  if(res==NULL)
    fprintf(stderr,"Array Object is NULL!\n");
  maps* tmp=t;
  while(tmp!=NULL){
    jsuint len;
    JSObject* res1=JS_NewObject(cx, NULL, NULL, NULL);
    JSObject *pval=JSObject_FromMap(cx,tmp->content);
    jsval pvalj=OBJECT_TO_JSVAL(pval);
    JS_SetProperty(cx, res1, tmp->name, &pvalj);
    JS_GetArrayLength(cx, res, &len);
    jsval res1j = OBJECT_TO_JSVAL(res1);
    JS_SetElement(cx,res,len,&res1j);
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
  while(tmpm!=NULL){
    jsval jsstr = STRING_TO_JSVAL(JS_NewString(cx,tmpm->value,strlen(tmpm->value)));
    JS_SetProperty(cx, res, tmpm->name,&jsstr);
#ifdef JS_DEBUG
    fprintf(stderr,"%s => %s\n",tmpm->name,tmpm->value);
#endif
    tmpm=tmpm->next;
  }
  return res;
}

maps* mapsFromJSObject(JSContext *cx,jsval t){
  maps *res=NULL;
  maps *tres=NULL;
  jsint oi=0;
  JSObject* tt=JSVAL_TO_OBJECT(t);
#ifdef JS_DEBUG
  fprintf(stderr,"Is finally an array ?\n");
  if(JS_IsArrayObject(cx,tt)){
    fprintf(stderr,"Is finally an array !\n");
  }
  else
    fprintf(stderr,"Is not an array !\n");
#endif
  jsint len;
  JSBool hasLen=JS_GetArrayLength(cx, tt, &len);
  if(hasLen==JS_FALSE){
#ifdef JS_DEBUG
    fprintf(stderr,"outputs array is empty\n");
#endif
  }
#ifdef JS_DEBUG
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
	fprintf(stderr,"Enumerate id : %d => %s\n",oi,JS_GetStringBytes(jsmsg));
#endif
	jsval nvp=JSVAL_NULL;
	if((JS_GetProperty(cx, JSVAL_TO_OBJECT(tmp1), JS_GetStringBytes(jsmsg), &nvp)==JS_FALSE))
#ifdef JS_DEBUG
	fprintf(stderr,"Enumerate id : %d => %s => No more value\n",oi,JS_GetStringBytes(jsmsg));
#endif
	if(JSVAL_IS_OBJECT(nvp)){
#ifdef JS_DEBUG
	  fprintf(stderr,"JSVAL NVP IS OBJECT\n");
#endif
	}
#ifdef JS_DEBUG
	else
	  fprintf(stderr,"JSVAL NVP IS NOT OBJECT !!\n");
#endif
	JSObject *nvp1;
	JS_ValueToObject(cx,nvp,&nvp1);
	jsval nvp1j=OBJECT_TO_JSVAL(nvp1);
	if(JSVAL_IS_OBJECT(nvp1j)){
#ifdef JS_DEBUG
	  fprintf(stderr,"JSVAL NVP1J IS OBJECT\n");
#endif
	  tres=(maps*)malloc(MAPS_SIZE);
	  tres->name=strdup(JS_GetStringBytes(jsmsg));
	  tres->content=mapFromJSObject(cx,nvp1j);
	  tres->next=NULL;
#ifdef JS_DEBUG
	  dumpMaps(res);
#endif
	  if(res==NULL)
	    res=dupMaps(&tres);
	  else
	    addMapsToMaps(&res,tres);
	  freeMaps(&tres);
	  free(tres);
	  tres=NULL;
#ifdef JS_DEBUG
	  dumpMaps(res);
#endif
	}
#ifdef JS_DEBUG
	else
	  fprintf(stderr,"JSVAL NVP1J IS NOT OBJECT !!\n");
#endif
      }
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
      JS_GetProperty(cx, JSVAL_TO_OBJECT(t), JS_GetStringBytes(jsmsg), &nvp);
      jsmsg1 = JS_ValueToString(cx,nvp);
      len1 = JS_GetStringLength(jsmsg1);
#ifdef JS_DEBUG
      fprintf(stderr,"Enumerate id : %d [ %s => %s ]\n",index,JS_GetStringBytes(jsmsg),JS_GetStringBytes(jsmsg1));
#endif
      if(res!=NULL){
#ifdef JS_DEBUG
	fprintf(stderr,"%s - %s\n",JS_GetStringBytes(jsmsg),JS_GetStringBytes(jsmsg1));
#endif
	addToMap(res,JS_GetStringBytes(jsmsg),JS_GetStringBytes(jsmsg1));
      }
      else{
	res=createMap(JS_GetStringBytes(jsmsg),JS_GetStringBytes(jsmsg1));
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

