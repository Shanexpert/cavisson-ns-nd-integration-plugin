// Start of All TR069 Parameters

// Start Inform Parameters
   // Request ID
   nsl_decl_var(TR069CPEReqIDDP);

   // Device info block
   nsl_decl_var(TR069DeviceInfoDP);

   // Search by netstorm intenaly
   nsl_decl_var(TR069ACSReqIDSP);

   // Used iternally by netstorm
   nsl_decl_var(TR069SPVStatusDP);
 
   // Declare var which will be used to save Url & send it in inform request 
   nsl_decl_var(TR069CpeUrlDP);

   // Request Headers
   nsl_decl_var(TR069ReqHeadersDP);

   // Current time
   nsl_date_var(TR069CurrentTimeDTP, Format=%FT%TZ, Refresh=SESSION);

   // Inform Event
   nsl_decl_var(TR069IEStructDP);

  //TransferComplete
  nsl_decl_var(TR069CmdKeyDP);

   // Inform Parameter Value
   nsl_decl_var(TR069IPVStructDP);
  
  // Number of events in infor request
  nsl_decl_var(TR069NumEventsDP);

  // Number of parameter values sent in Inform request
  nsl_decl_var(TR069NumIPVDP);

  nsl_static_var (TR069ManufacturerFP, TR069OUIFP, TR069ProductClassFP, TR069SerialNumberFP, TR069UsernameFP,TR069PasswordFP, TR069RPCSupportedFP, FILE=/home/netstorm/work/etc/tr069/data/tr069_device.dat, REFRESH=USE, MODE=SEQUENTIAL, FirstDataLine=2, HeaderLine=1, VAR_VALUE=F7=file);

   nsl_static_var (TR069IEFP, FILE=/home/netstorm/work/etc/tr069/data/tr069_inform_events.dat, REFRESH=USE, MODE=SEQUENTIAL, FirstDataLine=2, HeaderLine=1, VAR_VALUE=F1=file);
   nsl_static_var (TR069IPFP, FILE=/home/netstorm/work/etc/tr069/data/tr069_inform_parameters.dat, REFRESH=USE, MODE=SEQUENTIAL, FirstDataLine=2, HeaderLine=1, VAR_VALUE=F1=file);

// End Inform Parameters


// Start GetRPCMethods Parameters

   // Number of RPC methods
   nsl_decl_var(TR069NumRPCMethodsDP);

   //GetRPCMethods Response
   nsl_decl_var(TR069GetRPCMethodsResponseDP);

// End GetRPCMethods Parameters


// Start SetParameterValues (SPV) Parameters

// End - Parameters related to SetParameterValues (SPV)

// Start GetParameterValues (GPV) Parameters

   nsl_decl_var(TR069GPVStructDP);
   nsl_decl_var(TR060NumGPVDP);

   nsl_static_var (TR069GPVFP, FILE=/home/netstorm/work/etc/tr069/data/tr069_get_parameter_values.dat, REFRESH=USE, MODE=SEQUENTIAL, FirstDataLine=2, HeaderLine=1, VAR_VALUE=F1=file);
// End GetParameterValues (GPV) Parameters


// Start GetParameterNames (GPN) Parameters
   nsl_decl_var(TR069NumGPNDP); 
   nsl_decl_var(TR069GPNInfoStructDP); 
   
   nsl_static_var(TR069GPNFP,  FILE=/home/netstorm/work/etc/tr069/data/tr069_get_parameter_names.dat, REFRESH=USE, MODE=SEQUENTIAL, FirstDataLine=2, HeaderLine=1, VAR_VALUE=F1=file);
// End GetParameterNames (GPN) Parameters

// Start Get Parameter Attribute File Parameter
   nsl_decl_var(TR069GPAStructDP);
   nsl_decl_var(TR069NumGPAStructsDP);
   nsl_static_var (TR069GPAListFP, FILE=/home/netstorm/work/etc/tr069/data/tr069_get_parameter_attributes_list.dat, REFRESH=USE, MODE=SEQUENTIAL, FirstDataLine=2, HeaderLine=1, VAR_VALUE=F1=file);
// End Get Parameter Attribute File Parameter

// Start AddObject File Parameter
   nsl_static_var (TR069AddObjectInstanceNumberFP,TR069AddObjectStatusFP, FILE=/home/netstorm/work/etc/tr069/data/tr069_add_object_value.dat, REFRESH=USE, MODE=SEQUENTIAL, FirstDataLine=2, HeaderLine=1);
// End AddObject File Parameter


// Start Download Parameter
   nsl_date_var(TR069DownloadStartTimeDTP, Format="%r/%m-%d-%y", Refresh=USE);
   nsl_date_var(TR069DownloadCompleteTimeDTP, Format="%r/%m-%d-%y", Refresh=USE);
   nsl_static_var (TR069DownloadResponseStatusFP, FILE=/home/netstorm/work/etc/tr069/data/tr069_download_status.dat, REFRESH=USE, MODE=SEQUENTIAL, FirstDataLine=2, HeaderLine=1);
// End Download File Parameter


// Start DeleteObject Parameter
   nsl_static_var (TR069DeleteObjectStatusFP, FILE=/home/netstorm/work/etc/tr069/data/tr069_delete_object.dat, REFRESH=USE, MODE=SEQUENTIAL, FirstDataLine=2, HeaderLine=1);
// End DeleteObject File Parameter

// TR069 Search Vars

// Extract Body part of the SOAP response

// End of All TR069 Parameters
