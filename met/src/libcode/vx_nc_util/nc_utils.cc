// *=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
// ** Copyright UCAR (c) 1992 - 2022
// ** University Corporation for Atmospheric Research (UCAR)
// ** National Center for Atmospheric Research (NCAR)
// ** Research Applications Lab (RAL)
// ** P.O.Box 3000, Boulder, Colorado, 80307-3000, USA
// *=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*

////////////////////////////////////////////////////////////////////////

using namespace std;

#include <string.h>
#include <cstring>
#include <sys/stat.h>

#include <netcdf>
using namespace netCDF;
using namespace netCDF::exceptions;

#include "vx_log.h"
#include "nc_utils.h"
#include "util_constants.h"
#include "vx_cal.h"

////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////

void patch_nc_name(string *var_name) {
   size_t offset;

   // Replace commas with underscores
   offset = var_name->find(',');
   while (offset != string::npos) {
      var_name->replace(offset, 1, "_");
      offset = var_name->find(',', offset);
   }

   // Replaces stars with the word all
   offset = var_name->find('*');
   while (offset != string::npos) {
      var_name->replace(offset, 1, "all");
      offset = var_name->find('*', offset);
   }
}

////////////////////////////////////////////////////////////////////////

bool get_att_value(const NcAtt *att, ConcatString &value) {
   bool status = false;
   if (IS_VALID_NC_P(att)) {
      att->getValues(&value);
      status = true;
   }
   return status;
}

////////////////////////////////////////////////////////////////////////

template <typename T>
bool _get_att_num_value(const NcAtt *att, T &att_val, int matching_type) {
   bool status = false;
   if (IS_VALID_NC_P(att)) {
      int nc_type_id = GET_NC_TYPE_ID_P(att);
      if (matching_type == nc_type_id) {
         att->getValues(&att_val);
         status = true;
      }
      else if (NC_CHAR == nc_type_id) {
         string att_value;
         att->getValues(att_value);
         if (matching_type == NC_FLOAT)
            att_val = atof(att_value.c_str());
         else if (matching_type == NC_DOUBLE)
            att_val = (double)atof(att_value.c_str());
         else // if (matching_type == NC_INT)
            att_val = atoi(att_value.c_str());
         status = true;
      }
   }
   return(status);
}

////////////////////////////////////////////////////////////////////////

bool get_att_value(const NcAtt *att, ncbyte &att_val) {
   bool status = _get_att_num_value(att, att_val, NC_BYTE);
   return(status);
}

////////////////////////////////////////////////////////////////////////

bool get_att_value(const NcAtt *att, short &att_val) {
   bool status = _get_att_num_value(att, att_val, NC_SHORT);
   return(status);
}

////////////////////////////////////////////////////////////////////////

bool get_att_value(const NcAtt *att, int &att_val) {
   bool status = _get_att_num_value(att, att_val, NC_INT);
   return(status);
}

////////////////////////////////////////////////////////////////////////

bool get_att_value(const NcAtt *att, unsigned int &att_val) {
   bool status = _get_att_num_value(att, att_val, NC_UINT);
   return(status);
}

////////////////////////////////////////////////////////////////////////

bool get_att_value(const NcAtt *att, float &att_val) {
   bool status = _get_att_num_value(att, att_val, NC_FLOAT);
   return(status);
}

////////////////////////////////////////////////////////////////////////

bool get_att_value(const NcAtt *att, double &att_val) {
   bool status = _get_att_num_value(att, att_val, NC_DOUBLE);
   return(status);
}

////////////////////////////////////////////////////////////////////////

int get_att_value_int(const NcAtt *att) {
   int value = bad_data_int;
   static const char *method_name = "get_att_value_int(NcAtt) -> ";

   if (IS_INVALID_NC_P(att)) return value;

   switch (att->getType().getId()) {
      case NC_BYTE:
         ncbyte b_value;
         att->getValues(&b_value);
         value = (int)b_value;
         break;
      case NC_SHORT:
         short s_value;
         att->getValues(&s_value);
         value = (int)s_value;
         break;
      case NC_INT:
         att->getValues(&value);
         break;
      case NC_INT64:
         long long l_value;
         att->getValues(&l_value);
         value = (int)l_value;
         if ((long long)value != l_value) {
            mlog << Warning << "\n" << method_name
                 << "loosing precision during type conversion. "
                 << value << " from int64 \"" << l_value
                 << "\" for attribute \"" << GET_SAFE_NC_NAME_P(att) << "\".\n\n";
         }
         break;
      default:
         mlog << Warning << "\n" << method_name
              << "data type mismatch (int vs. \"" << GET_NC_TYPE_NAME_P(att)
              << "\" for attribute \"" << GET_SAFE_NC_NAME_P(att) << "\".\n\n";
         break;
   }
   return value;
}

////////////////////////////////////////////////////////////////////////

char get_att_value_char(const NcAtt *att) {
   char att_val = bad_data_char;
   static const char *method_name = "get_att_value_char(NcAtt) -> ";
   if (IS_VALID_NC_P(att)) {
      nc_type attType = GET_NC_TYPE_ID_P(att);
      if (attType == NC_CHAR) {
         att->getValues(&att_val);
      }
      else {
         mlog << Error << "\n" << method_name
              << "Please convert data type of \"" << GET_NC_NAME_P(att)
              << "\" to NC_CHAR type.\n\n";
         exit(1);
      }
   }
   return att_val;
}

////////////////////////////////////////////////////////////////////////

bool get_att_value_chars(const NcAtt *att, ConcatString &value) {
   bool status = false;
   static const char *method_name = "get_att_value_chars(NcAtt) -> ";
   if (IS_VALID_NC_P(att)) {
      nc_type attType = GET_NC_TYPE_ID_P(att);
      if (attType == NC_CHAR || attType == NC_STRING) {
         try {
            string att_value;
            att->getValues(att_value);
            value = att_value;
         }
         catch (exceptions::NcChar ex) {
            value = "";
            // Handle netCDF::exceptions::NcChar:  NetCDF: Attempt to convert between text & numbers
            mlog << Warning << "\n" << method_name
                 << "Exception: " << ex.what() << "\n"
                 << "Fail to read " << GET_NC_NAME_P(att) << " attribute ("
                 << GET_NC_TYPE_NAME_P(att) << " type).\n"
                 << "Please check the encoding of the "<< GET_NC_NAME_P(att) << " attribute.\n\n";
         }
      }
      else { // MET-788: to handle a custom modified NetCDF
         mlog << Error << "\n" << method_name
              << "Please convert data type of \"" << GET_NC_NAME_P(att)
              << "\" " << GET_NC_TYPE_NAME_P(att) << " to NC_CHAR type.\n\n";
         exit(1);
      }
      status = true;
   }
   return status;
}

////////////////////////////////////////////////////////////////////////

long long get_att_value_llong(const NcAtt *att) {
   long long value = bad_data_int;
   if (IS_VALID_NC_P(att)) att->getValues(&value);
   return value;
}

////////////////////////////////////////////////////////////////////////

double get_att_value_double(const NcAtt *att) {
   double value = bad_data_double;
   if (IS_VALID_NC_P(att)) att->getValues(&value);
   return value;
}

////////////////////////////////////////////////////////////////////////

void get_att_value_doubles(const NcAtt *att, NumArray &value) {
   value.erase();
   double *values = new double[att->getAttLength()];
   att->getValues(values);
   for(unsigned int i=0; i<=att->getAttLength(); i++) value.add(values[i]);
   if(values) { delete [] values; values = 0; }
   return;
}

double *get_att_value_doubles(const NcAtt *att) {
   double *values = new double[att->getAttLength()];
   att->getValues(values);
   return values;
}

////////////////////////////////////////////////////////////////////////

float get_att_value_float(const NcAtt *att) {
   float value = bad_data_float;
   att->getValues(&value);
   return value;
}

////////////////////////////////////////////////////////////////////////

short get_att_value_short(const NcAtt *att) {
   short value = bad_data_int;
   att->getValues(&value);
   return value;
}

unsigned short get_att_value_ushort(const NcAtt *att) {
   unsigned short value = bad_data_int;
   att->getValues(&value);
   return value;
}

////////////////////////////////////////////////////////////////////////

bool get_att_value_string(const NcVar *var, const ConcatString &att_name, ConcatString &value) {
   NcVarAtt *att = get_nc_att(var, att_name);
   bool status =  get_att_value_chars(att, value);
   if (att) delete att;
   return status;
}

////////////////////////////////////////////////////////////////////////

int  get_att_value_int   (const NcVar *var, const ConcatString &att_name) {
   NcVarAtt *att = get_nc_att(var, att_name);
   int att_val = get_att_value_int(att);
   if (att) delete att;
   return att_val;
}

////////////////////////////////////////////////////////////////////////

long long  get_att_value_llong (const NcVar *var, const ConcatString &att_name) {
   NcVarAtt *att = get_nc_att(var, att_name);
   long long att_val = get_att_value_llong(att);
   if (att) delete att;
   return att_val;
}

////////////////////////////////////////////////////////////////////////

double get_att_value_double(const NcVar *var, const ConcatString &att_name) {
   NcVarAtt *att = get_nc_att(var, att_name);
   double att_val = get_att_value_double(att);
   if (att) delete att;
   return att_val;
}

////////////////////////////////////////////////////////////////////////

bool get_att_value_string(const NcFile *nc, const ConcatString &att_name, ConcatString &value) {
   NcGroupAtt *att = get_nc_att(nc, att_name);
   bool status = get_att_value_chars(att, value);
   if (att) delete att;
   return status;
}

////////////////////////////////////////////////////////////////////////

int  get_att_value_int   (const NcFile *nc, const ConcatString &att_name) {
   NcGroupAtt *att = get_nc_att(nc, att_name);
   int att_val = get_att_value_int(att);
   if (att) delete att;
   return att_val;
}

////////////////////////////////////////////////////////////////////////

long long  get_att_value_llong (const NcFile *nc, const ConcatString &att_name) {
   NcGroupAtt *att = get_nc_att(nc, att_name);
   long long att_val = get_att_value_llong(att);
   if (att) delete att;
   return att_val;
}

////////////////////////////////////////////////////////////////////////

double  get_att_value_double(const NcFile *nc, const ConcatString &att_name) {
   NcGroupAtt *att = get_nc_att(nc, att_name);
   double att_val = get_att_value_double(att);
   if (att) delete att;
   return att_val;
}

////////////////////////////////////////////////////////////////////////

bool    get_att_no_leap_year(const NcVar *var) {
   bool no_leap_year = false;
   NcVarAtt *calendar_att = get_nc_att(var, string("calendar"), false);
   if (IS_VALID_NC_P(calendar_att)) {
      ConcatString calendar_value;
      if (get_att_value_chars(calendar_att, calendar_value)) {
         no_leap_year = ( "noleap" == calendar_value
                        || "365_day" == calendar_value
                        || "365 days" == calendar_value);
      }
   }
   if (calendar_att) delete calendar_att;
   return no_leap_year;
}

////////////////////////////////////////////////////////////////////////

ConcatString get_log_msg_for_att(const NcVarAtt *att) {
   ConcatString log_msg("can't read attribute");
   if(IS_INVALID_NC_P(att)) {
      log_msg << " because attribute does not exist";
   }
   else {
      log_msg << " \"" << GET_NC_NAME_P(att) << "\" from \""
              << GET_SAFE_NC_NAME(att->getParentVar()) << "\" variable.\n\n";
   }
   log_msg << ".\n\n";
   return(log_msg);
}

////////////////////////////////////////////////////////////////////////

ConcatString get_log_msg_for_att(const NcVarAtt *att, string var_name,
                             const ConcatString att_name) {
   ConcatString log_msg;
   log_msg << "can't read attribute" << " \""
           << ((att_name.length() > 0) ? att_name.c_str() : GET_SAFE_NC_NAME_P(att))
           << "\" because attribute does not exist";
   if (0 != var_name.compare(C_unknown_str)) {
      log_msg << " from \"" << var_name << "\" variable";
   }
   else {
      if(IS_VALID_NC_P(att)) {
         log_msg << " from \"" << GET_SAFE_NC_NAME(att->getParentVar()) << "\" variable";
      }
   }
   log_msg << ".\n\n";
   return(log_msg);
}

////////////////////////////////////////////////////////////////////////

NcVarAtt *get_nc_att(const NcVar * var, const ConcatString &att_name, bool exit_on_error) {
   NcVarAtt *att = (NcVarAtt *)0;
   static const char *method_name = "get_nc_att(NcVar) -> ";

   //
   // Retrieve the NetCDF variable attribute.
   //
   if(IS_INVALID_NC_P(var)) {
      mlog << Error << "\n" << method_name
           << "can't read attribute \"" << att_name
           << "\" from because variable is invalid.\n\n";
   }
   else {
      multimap<string,NcVarAtt>::iterator itAtt;
      map<string,NcVarAtt> mapAttrs = var->getAtts();
      for (itAtt = mapAttrs.begin(); itAtt != mapAttrs.end(); ++itAtt) {
         if ( att_name == (*itAtt).first) {
            att = new NcVarAtt();
            *att = (*itAtt).second;
            break;
         }
      }

      if(IS_INVALID_NC_P(att) && exit_on_error) {
         mlog << Error << "\n" << method_name
              << "can't read attribute \"" << att_name
              << "\" from \"" << var->getName() << "\" variable.\n\n";
         exit(1);
      }
   }
   return(att);
}

////////////////////////////////////////////////////////////////////////

NcGroupAtt *get_nc_att(const NcFile * nc, const ConcatString &att_name, bool exit_on_error) {
   NcGroupAtt *att = (NcGroupAtt *)0;
   static const char *method_name = "get_nc_att(NcFile) -> ";

   //
   // Retrieve the NetCDF variable attribute.
   //
   if(IS_INVALID_NC_P(nc)) {
      mlog << Error << "\n" << method_name
           << "can't read attribute \"" << att_name
           << "\" from because NC is invalid.\n\n";
      if (exit_on_error) exit(1);
   }
   else {
      multimap<string,NcGroupAtt>::iterator itAtt;
      multimap<string,NcGroupAtt> mapAttrs = nc->getAtts();
      for (itAtt = mapAttrs.begin(); itAtt != mapAttrs.end(); ++itAtt) {
         if ( att_name == (*itAtt).first ) {
            att = new NcGroupAtt();
            *att = (*itAtt).second;
            break;
         }
      }

      if(IS_INVALID_NC_P(att) && exit_on_error) {
         mlog << Error << "\n" << method_name
              << "can't read attribute \"" << att_name
              << "\" from \"" << nc->getName() << "\".\n\n";
         exit(1);
      }
   }
   return(att);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_att_value(const NcVar *var, const ConcatString &att_name,
                      ConcatString &att_val, bool exit_on_error) {
   bool status = false;
   NcVarAtt *att = (NcVarAtt *) 0;

   // Initialize
   att_val.clear();

   att = get_nc_att(var, att_name);

   // Look for a match
   status = get_att_value_chars(att, att_val);
   if (att) delete att;

   return(status);
}

////////////////////////////////////////////////////////////////////////

template <typename T>
bool _get_nc_att_value(const NcVar *var, const ConcatString &att_name,
                       T &att_val, bool exit_on_error,
                       T bad_data, const char *caller_name) {
   bool status = false;

   // Initialize
   att_val = bad_data;

   //
   // Retrieve the NetCDF variable attribute.
   //
   NcVarAtt *att = get_nc_att(var, att_name);
   status = get_att_value((NcAtt *)att, att_val);
   if (!status) {
      mlog << Error << "\n" << caller_name
           << get_log_msg_for_att(att, GET_SAFE_NC_NAME_P(var), att_name);
      if (exit_on_error) {
         if (att) delete att;
         exit(1);
      }
   }
   if (att) delete att;

   return(status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_att_value(const NcVar *var, const ConcatString &att_name,
                      int &att_val, bool exit_on_error) {
   static const char *method_name = "get_nc_att_value(NcVar,int) -> ";
   bool status = _get_nc_att_value(var, att_name, att_val, exit_on_error,
                                   bad_data_int, method_name);
   return(status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_att_value(const NcVar *var, const ConcatString &att_name,
                      float &att_val, bool exit_on_error) {
   static const char *method_name = "get_nc_att_value(NcVar,float) -> ";
   bool status = _get_nc_att_value(var, att_name, att_val, exit_on_error,
                                   bad_data_float, method_name);
   return(status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_att_value(const NcVarAtt *att, ConcatString &att_val) {
   bool status = false;

   // Initialize
   att_val.clear();

   // Look for a match
   if(IS_VALID_NC_P(att)) {
      string attr_value;
      att->getValues(attr_value);
      att_val = attr_value.c_str();
      status = true;
   }

   return(status);
}

////////////////////////////////////////////////////////////////////////

template <typename T>
bool _get_nc_att_value(const NcVarAtt *att, T &att_val, bool exit_on_error,
                       T bad_data, const char *caller_name) {
   bool status = true;

   // Initialize
   att_val = bad_data;

   //
   // Retrieve the NetCDF variable attribute.
   //
   status = get_att_value((NcAtt *)att, att_val);
   if (!status) {
      mlog << Error << "\n" << caller_name << get_log_msg_for_att(att);

      if (exit_on_error) exit(1);
   }

   return(status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_att_value(const NcVarAtt *att, int &att_val, bool exit_on_error) {
   static const char *method_name = "get_nc_att_value(NcVarAtt,int) -> ";
   bool status = _get_nc_att_value(att, att_val, exit_on_error, bad_data_int, method_name);
   return(status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_att_value(const NcVarAtt *att, float &att_val, bool exit_on_error) {
   static const char *method_name = "get_nc_att_value(NcVarAtt,float) -> ";
   bool status = _get_nc_att_value(att, att_val, exit_on_error, bad_data_float, method_name);
   return(status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_att_value(const NcVarAtt *att, double &att_val, bool exit_on_error) {
   static const char *method_name = "get_nc_att_value(NcVarAtt,double) -> ";
   bool status = _get_nc_att_value(att, att_val, exit_on_error, bad_data_double, method_name);
   return(status);
}

///////////////////////////////////////////////////////////////////////////////

bool has_att(NcFile * ncfile, const ConcatString att_name, bool exit_on_error)
{
   bool status = false;
   NcGroupAtt *att;

   att = get_nc_att(ncfile, att_name);
   if ( IS_VALID_NC_P(att)) {
      status = true;
   } else if(exit_on_error)  {
      mlog << Error << "\nhas_att() -> "
           << "can't find global NetCDF attribute " << att_name
           << ".\n\n";
      exit ( 1 );
   }
   if (att) delete att;
   return status;
}

////////////////////////////////////////////////////////////////////////

bool has_unsigned_attribute(NcVar *var) {
   bool is_unsigned = false;
   static const char *method_name = "has_unsigned_attribute() -> ";
   NcVarAtt *att_unsigned     = get_nc_att(var, string("_Unsigned"));
   if (IS_VALID_NC_P(att_unsigned)) {
      ConcatString att_value;
      get_att_value_chars(att_unsigned, att_value);
      is_unsigned = ( att_value == "true" );
   }
   if(att_unsigned) delete att_unsigned;
   mlog << Debug(6) << method_name
        << GET_NC_NAME_P(var) << " " << (is_unsigned ? "has " : "does not have" )
        << " _Unsigned attribute.\n";

   return is_unsigned;
}

////////////////////////////////////////////////////////////////////////

bool get_global_att(const NcGroupAtt *att, ConcatString &att_val) {
   bool status = false;

   // Initialize
   att_val.clear();

   // Look for a match
   if(IS_VALID_NC_P(att)) {
      string attr_value;
      att->getValues(attr_value);
      att_val = attr_value.c_str();
      status = true;
   }

   return(status);
}

////////////////////////////////////////////////////////////////////////

bool get_global_att(const char *nc_name, const ConcatString &att_name,
                    ConcatString &att_val) {
   bool status = false;

   // Initialize
   att_val.clear();

   NcFile *nc = open_ncfile(nc_name);
   if (0 != nc && IS_VALID_NC_P(nc)) {
      status = get_global_att(nc, att_name, att_val, false);
   }

   if(nc) delete nc;

   return(status);
}

////////////////////////////////////////////////////////////////////////

bool get_global_att(const char *nc_name, const ConcatString &att_name,
                    bool &att_val) {
   bool status = false;

   // Initialize
   NcFile *nc = open_ncfile(nc_name);
   if (0 != nc && IS_VALID_NC_P(nc)) {
      status = get_global_att(nc, att_name, att_val, false);
   }

   if(nc) delete nc;

   return(status);
}

////////////////////////////////////////////////////////////////////////

bool get_global_att(const NcFile *nc, const ConcatString &att_name,
                    ConcatString &att_val, bool error_out) {
   bool status = false;
   NcGroupAtt *att;

   // Initialize
   att_val.clear();

   att = get_nc_att(nc, att_name);
   if(IS_VALID_NC_P(att)) {
      string attr_val;
      att->getValues(attr_val);
      att_val = attr_val.c_str();
      status = true;
   }
   if (att) delete att;

   // Check error_out status
   if(error_out && !status) {
      mlog << Error << "\nget_global_att(ConcatString) -> "
           << "can't find global NetCDF attribute \"" << att_name
           << "\".\n\n";
      exit(1);
   }

   return(status);
}

////////////////////////////////////////////////////////////////////////

template <typename T>
bool _get_global_att_value(const NcFile *nc, const ConcatString& att_name,
                           T &att_val, T bad_data, bool error_out, const char *caller_name) {
   bool status = false;
   // Initialize
   att_val = bad_data;

   NcGroupAtt *nc_att = get_nc_att(nc, att_name);
   if (IS_VALID_NC_P(nc_att)) {
      status = get_att_value((NcAtt *)nc_att, att_val);
      string data_type = GET_NC_TYPE_NAME_P(nc_att);
      if (error_out && !status) {
         mlog << Error << caller_name
              << "The data type \"" << data_type
              << "\" for \"" << att_name << "\" does not match...\n\n";
      }
   }
   else if (error_out) {
      mlog << Error << caller_name
           << "can't find global NetCDF attribute \"" << att_name
           << "\".\n\n";
   }
   if (nc_att) delete nc_att;
   // Check error_out status
   if (error_out && !status) exit(1);

   return(status);
}

////////////////////////////////////////////////////////////////////////


bool get_global_att(const NcFile *nc, const ConcatString& att_name,
                    int &att_val, bool error_out) {
   static const char *method_name = "\nget_global_att(int) -> ";
   bool status = _get_global_att_value(nc, att_name, att_val, bad_data_int,
                                       false, method_name);
   if (!status) {
      short tmp_att_val;
      status = _get_global_att_value(nc, att_name, tmp_att_val, (short)bad_data_int,
                                     false, method_name);
      if (status) att_val = tmp_att_val;
      else {
         ncbyte tmp_val2;
         status = _get_global_att_value(nc, att_name, tmp_val2, (ncbyte)bad_data_int,
                                        error_out, method_name);
         if (status) att_val = tmp_val2;
      }
   }

   return(status);
}

////////////////////////////////////////////////////////////////////////

bool get_global_att(const NcFile *nc, const ConcatString& att_name,
                    bool &att_val, bool error_out) {
   bool status;
   ConcatString att_value;
   static const char *method_name = "\nget_global_att(bool) -> ";

   // Initialize
   att_val = false;
   status = get_global_att(nc, att_name, att_value, error_out);
   if(status) {
      att_val = (0 == strcmp("true", att_value.text()))
             || (0 == strcmp("yes", att_value.text()));
   }
   else if(error_out) {
      // Check error_out status
      mlog << Error << method_name
           << "can't find global NetCDF attribute \"" << att_name << "\".\n\n";
      exit(1);
   }

   return(status);
}

////////////////////////////////////////////////////////////////////////

bool get_global_att(const NcFile *nc, const ConcatString& att_name,
                    float &att_val, bool error_out) {
   static const char *method_name = "\nget_global_att(float) -> ";
   bool status = _get_global_att_value(nc, att_name, att_val, bad_data_float,
                                       error_out, method_name);

   return(status);
}

////////////////////////////////////////////////////////////////////////

bool get_global_att(const NcFile *nc, const ConcatString& att_name,
                    double &att_val, bool error_out) {
   static const char *method_name = "\nget_global_att(double) -> ";
   bool status;
   status = _get_global_att_value(nc, att_name, att_val, bad_data_double,
                                      false, method_name);
   if (!status) {
      float tmp_att_val;
      status = _get_global_att_value(nc, att_name, tmp_att_val, bad_data_float,
                                     error_out, method_name);
      if (status) att_val = tmp_att_val;
   }

   return (status);
}

////////////////////////////////////////////////////////////////////////

int get_version_no(const NcFile *nc) {
   int version_no = 0;
   float att_version_no;
   get_global_att(nc, (const ConcatString)nc_att_obs_version, att_version_no);
   version_no = (int)(att_version_no * 100);
   return version_no;
}

////////////////////////////////////////////////////////////////////////

bool is_version_less_than_1_02(const NcFile *nc) {
   int version_no = get_version_no(nc);
   return (version_no < 102);
}

////////////////////////////////////////////////////////////////////////

void add_att(NcFile *nc, const string &att_name, const int att_val) {
   nc->putAtt(att_name, NcType::nc_INT, att_val);
}

////////////////////////////////////////////////////////////////////////

void add_att(NcFile *nc, const string &att_name, const string att_val) {
   nc->putAtt(att_name, att_val);
}

////////////////////////////////////////////////////////////////////////

void add_att(NcFile *nc, const string &att_name, const char *att_val) {
   nc->putAtt(att_name, att_val);
}

////////////////////////////////////////////////////////////////////////

void add_att(NcFile *nc, const string &att_name, const ConcatString att_val) {
  nc->putAtt(att_name, att_val.text());
}

////////////////////////////////////////////////////////////////////////

void add_att(NcVar *var, const string &att_name, const string att_val) {
   var->putAtt(att_name, att_val);
}

////////////////////////////////////////////////////////////////////////

void add_att(NcVar *var, const string &att_name, const int att_val) {
   var->putAtt(att_name, NcType::nc_INT, att_val);
}

////////////////////////////////////////////////////////////////////////

void add_att(NcVar *var, const string &att_name, const float att_val) {
   var->putAtt(att_name, NcType::nc_FLOAT, att_val);
}

////////////////////////////////////////////////////////////////////////

void add_att(NcVar *var, const string &att_name, const double att_val) {
   var->putAtt(att_name, NcType::nc_DOUBLE, att_val);
}


////////////////////////////////////////////////////////////////////////

int get_var_names(NcFile *nc, StringArray *varNames) {

   int i, varCount;
   NcVar var;

   varCount = nc->getVarCount();

   i = 0;
   multimap<string,NcVar>::iterator itVar;
   multimap<string,NcVar> mapVar = GET_NC_VARS_P(nc);
   for (itVar = mapVar.begin(); itVar != mapVar.end(); ++itVar) {
      var = (*itVar).second;
      varNames->add(var.getName());
      i++;
   }

   if (i != varCount) {
      mlog << Error << "\n\tget_var_names() -> "
           << "does not match array, allocated " << varCount << " but assigned "
           << i << ".\n\n";
   }
   return(varCount);
}

////////////////////////////////////////////////////////////////////////

template <typename T>
bool _get_var_att_num(const NcVar *var, const ConcatString &att_name,
                      T &att_val, T bad_data) {
   bool status = false;

   // Initialize
   att_val = bad_data;

   NcVarAtt *att = get_nc_att(var, att_name);
   // Look for a match
   if (IS_VALID_NC_P(att)) {
      att->getValues(&att_val);
      status = true;
   }
   if (att) delete att;

   return(status);
}

////////////////////////////////////////////////////////////////////////

bool get_var_att_double(const NcVar *var, const ConcatString &att_name,
                        double &att_val) {
   bool status = _get_var_att_num(var, att_name, att_val, bad_data_double);

   return(status);
}

////////////////////////////////////////////////////////////////////////

bool get_var_att_float(const NcVar *var, const ConcatString &att_name,
                       float &att_val) {
   bool status = _get_var_att_num(var, att_name, att_val, bad_data_float);

   return(status);
}

////////////////////////////////////////////////////////////////////////

bool get_var_units(const NcVar *var, ConcatString &att_val) {

   return(get_nc_att_value(var, units_att_name, att_val));
}

////////////////////////////////////////////////////////////////////////

bool get_var_level(const NcVar *var, ConcatString &att_val) {

   return(get_nc_att_value(var, level_att_name, att_val));
}

////////////////////////////////////////////////////////////////////////

double get_var_missing_value(const NcVar *var) {
   double v;

   if(!get_var_att_double(var, missing_value_att_name, v)) {
      v = bad_data_double;
   }

   return(v);
}

////////////////////////////////////////////////////////////////////////

double get_var_fill_value(const NcVar *var) {
   double v;

   if(!get_var_att_double(var, fill_value_att_name, v)) {
      v = bad_data_double;
   }

   return(v);
}

////////////////////////////////////////////////////////////////////////

char get_char_val(NcFile * nc, const char * var_name, const int index) {
   NcVar var = get_var(nc, var_name);
   return (get_char_val(&var, index));
}

////////////////////////////////////////////////////////////////////////

char get_char_val(NcVar *var, const int index) {
   char k;
   std::vector<size_t> start;
   std::vector<size_t> count;

   //
   // Retrieve the character array value from the NetCDF variable.
   //
   start.push_back(index);
   count.push_back(1);
   var->getVar(start, count, &k);

   return (k);
}

////////////////////////////////////////////////////////////////////////

ConcatString* get_string_val(NcFile * nc, const char * var_name, const int index,
                             const int len, ConcatString &tmp_cs) {
   NcVar var = get_var(nc, var_name);

   return (get_string_val(&var, index, len, tmp_cs));
}

////////////////////////////////////////////////////////////////////////

ConcatString* get_string_val(NcVar *var, const int index,
                             const int len, ConcatString &tmp_cs) {
   char tmp_str[len];
   std::vector<size_t> start;
   std::vector<size_t> count;
   const char *method_name = "get_string_val() ";

   if (2 != get_dim_count(var)) {
      mlog << Error << "\n" << method_name << GET_NC_NAME_P(var)
           << " is not a two dimensional variablle. start offset and count: ("
           << index << ", " << len << ").\n\n";
      exit(1);
   }
   else {
      int dim_size1 = get_dim_size(var, 0);
      int dim_size2 = get_dim_size(var, 1);
      if ((index > dim_size1) || (len > dim_size2)) {
         mlog << Error << "\n" << method_name << "The start offset and count ("
              << index << ", " << len << ") exceeds the dimension size ("
              << dim_size1 << ", " << dim_size2 << ") for the variable "
              << GET_NC_NAME_P(var) << ".\n\n";
         exit(1);
      }
   }

   //
   // Retrieve the character array value from the NetCDF variable.
   //
   start.push_back(index);
   start.push_back(0);
   count.push_back(1);
   count.push_back(len);
   var->getVar(start, count, &tmp_str);

   //
   // Store the character array as a ConcatString
   //
   tmp_cs = tmp_str;

   return (&tmp_cs);
}

////////////////////////////////////////////////////////////////////////

int get_int_var(NcFile * nc, const char * var_name, const int index) {
   NcVar var = get_var(nc, var_name);
   return(get_int_var(&var, index));
}

////////////////////////////////////////////////////////////////////////

int get_int_var(NcVar * var, const int index) {
   int k;
   std::vector<size_t> start;
   std::vector<size_t> count;
   const char *method_name = "get_int_var() ";

   k = bad_data_int;
   if (IS_VALID_NC_P(var)) {
      int dim_idx = 0;
      int dim_size = get_dim_size(var, dim_idx);
      if (0 >= dim_size) {
         if ((index > 0) && (0 >= dim_size)) {
            mlog << Error << "\n" << method_name << "The start offset ("
                 << index << ") should be 0 because of no dimension at the variable "
                 << GET_NC_NAME_P(var) << ".\n\n";
            exit(1);
         }
      }
      else if (index > dim_size) {
         NcDim nc_dim = get_nc_dim(var, dim_idx);
         mlog << Error << "\n" << method_name << "The start offset ("
              << index << ") exceeds the dimension " << dim_size << " "
              << (IS_VALID_NC(nc_dim) ? GET_NC_NAME(nc_dim) : " ")
              << " for the variable " << GET_NC_NAME_P(var) << ".\n\n";
         exit(1);
      }

      start.push_back(index);
      count.push_back(1);
      var->getVar(start, count, &k);
   }

   return(k);
}

////////////////////////////////////////////////////////////////////////

double get_nc_time(NcVar * var, const int index) {
   double k;
   std::vector<size_t> start;
   std::vector<size_t> count;
   const char *method_name = "get_nc_time() -> ";

   k = bad_data_double;
   if (IS_VALID_NC_P(var)) {
      int dim_idx = 0;
      int dim_size = get_dim_size(var, dim_idx);
      if (0 >= dim_size) {
         if (index > 0) {
            mlog << Error << "\n" << method_name << "The start offset ("
                 << index << ") should be 0 because of no dimension at the variable "
                 << GET_NC_NAME_P(var) << ".\n\n";
            exit(1);
         }
      }
      else if (index > dim_size) {
         NcDim nc_dim = get_nc_dim(var, dim_idx);
         mlog << Error << "\n" << method_name << "The start offset ("
              << index << ") exceeds the dimension " << dim_size << " "
              << (IS_VALID_NC(nc_dim) ? GET_NC_NAME(nc_dim) : " ")
              << " for the variable " << GET_NC_NAME_P(var) << ".\n\n";
         exit(1);
      }

      start.push_back(index);
      count.push_back(1);

      int vi;
      short vs;
      float vf;
      ncbyte vb;
      long long vl;
      int dataType = GET_NC_TYPE_ID_P(var);
      switch (dataType) {
         case NC_DOUBLE:
            var->getVar(start, count, &k);
            break;
         case NC_FLOAT:
            var->getVar(start, count, &vf);
            k = (double)vf;
            break;
         case NC_SHORT:
            var->getVar(start, count, &vs);
            k = (double)vs;
            break;
         case NC_BYTE:
            var->getVar(start, count, &vb);
            k = (double)vb;
            break;
         case NC_INT:
            var->getVar(start, count, &vi);
            k = (double)vi;
            break;
         case NC_INT64:
            long long converted_vl;
            var->getVar(start, count, &vl);
            k = (double)vl;
            converted_vl = (long long)k;
            if (converted_vl != vl) {
               mlog << Warning << "\n" << method_name
                    << " the value was changed during type conversion: "
                    << converted_vl << " (was  " << vl << ")\n";
            }
            break;
         default:
            mlog << Error << "\n" << method_name
                 << "data type mismatch (double vs. \"" << GET_NC_TYPE_NAME_P(var)
                 << "\").\nPlease correct the data type to double for variable \""
                 << GET_NC_NAME_P(var) << "\".\n\n";
            exit(1);
      }
   }

   return(k);
}

////////////////////////////////////////////////////////////////////////

float get_float_var(NcFile * nc, const char * var_name, const int index) {
   NcVar var = get_var(nc, var_name);
   return(get_float_var(&var, index));
}

////////////////////////////////////////////////////////////////////////

float get_float_var(NcVar * var, const int index) {
   float k;
   std::vector<size_t> start;
   std::vector<size_t> count;
   const char *method_name = "get_float_var() -> ";

   k = bad_data_float;
   if (IS_VALID_NC_P(var)) {
      int dim_idx = 0;
      int dim_size = get_dim_size(var, dim_idx);
      if (0 >= dim_size) {
         if (index > 0) {
            mlog << Error << "\n" << method_name << "The start offset ("
                 << index << ") should be 0 because of no dimension at the variable "
                 << GET_NC_NAME_P(var) << ".\n\n";
            exit(1);
         }
      }
      else if ((index > dim_size) && (0 < dim_size)){
         NcDim nc_dim = get_nc_dim(var, dim_idx);
         mlog << Error << "\n" << method_name << "The start offset ("
              << index << ") exceeds the dimension " << dim_size << " "
              << (IS_VALID_NC(nc_dim) ? GET_NC_NAME(nc_dim) : " ")
              << " for the variable " << GET_NC_NAME_P(var) << ".\n\n";
         exit(1);
      }

      start.push_back(index);
      count.push_back(1);
      var->getVar(start, count, &k);
   }

   return(k);
}

////////////////////////////////////////////////////////////////////////

template <typename T>
bool _get_nc_data(NcFile *nc, const char *var_name, T *data,
                 const long *dim, const long *cur) {

   //
   // Retrieve the input variables
   //
   NcVar var = get_var(nc, var_name);
   return get_nc_data(&var, data, dim, cur);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data(NcFile *nc, const char *var_name, int *data,
                 const long *dim, const long *cur) {

   return _get_nc_data(nc, var_name, data, dim, cur);
}

////////////////////////////////////////////////////////////////////////

template <typename T>
bool _get_nc_data(NcVar *var, T *data) {
   bool return_status = false;

   if (IS_VALID_NC_P(var)) {
      //
      // Retrieve the float value from the NetCDF variable.
      // Note: missing data was checked here
      //
      var->getVar(data);
      return_status = true;
   }
   return(return_status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data(NcVar *var, time_t *data) {
   bool return_status = _get_nc_data(var, data);
   return(return_status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data(NcVar *var, int *data) {
   bool return_status = _get_nc_data(var, data);
   return(return_status);
}

////////////////////////////////////////////////////////////////////////

template <typename T>
bool _get_nc_data(NcVar *var, T *data, T bad_data, const long *curs) {
   bool return_status = false;
   const char *method_name = "_get_nc_data(const long *curs) ";

   if (IS_VALID_NC_P(var)) {
      std::vector<size_t> start;
      std::vector<size_t> count;

      const int dimC = get_dim_count(var);
      for (int idx = 0 ; idx < dimC; idx++) {
         int dim_size = get_dim_size(var, idx);
         if ((curs[idx] > dim_size) && (0 < dim_size)) {
            NcDim nc_dim = get_nc_dim(var, idx);
            mlog << Error << "\n" << method_name << "The start offset ("
                 << curs[idx] << ") exceeds the dimension[" << idx << "] " << dim_size << " "
                 << (IS_VALID_NC(nc_dim) ? GET_NC_NAME(nc_dim) : " ")
                 << " for the variable " << GET_NC_NAME_P(var) << ".\n\n";
            exit(1);
         }
         start.push_back((size_t)curs[idx]);
         count.push_back((size_t)1);
      }

      *data = bad_data;

      //
      // Retrieve the float value from the NetCDF variable.
      // Note: missing data was checked here
      //
      var->getVar(start, count, data);
      return_status = true;
   }
   return(return_status);
}

////////////////////////////////////////////////////////////////////////


bool get_nc_data(NcVar *var, int *data, const long *curs) {
   bool return_status = _get_nc_data(var, data, bad_data_int, curs);

   return(return_status);
}

////////////////////////////////////////////////////////////////////////

template <typename T>
bool _get_nc_data(NcVar *var, T *data, T bad_data, const long dim, const long cur) {
   bool return_status = false;
   const char *method_name = "_get_nc_data(const long dim, const long cur) ";

   if (IS_VALID_NC_P(var)) {
      int dim_idx = 0;
      std::vector<size_t> start;
      std::vector<size_t> count;
      start.push_back((size_t)cur);
      count.push_back((size_t)dim);
      int dim_size = get_dim_size(var, dim_idx);
      if (0 >= dim_size) {
         if ((cur > 0) || (dim > 1)) {
            mlog << Error << "\n" << method_name << "The start offset and count ("
                 << cur << ", " << dim << ") should be (0, 1) because of no dimension at the variable "
                 << GET_NC_NAME_P(var) << ".\n\n";
            exit(1);
         }
      }
      else if (((cur+dim) > dim_size) && (0 < dim_size)) {
         NcDim nc_dim = get_nc_dim(var, dim_idx);
         mlog << Error << "\n" << method_name << "The start offset and count ("
              << cur << " + " << dim << ") exceeds the dimension " << dim_size << " "
              << (IS_VALID_NC(nc_dim) ? GET_NC_NAME(nc_dim) : " ")
              << " for the variable " << GET_NC_NAME_P(var) << ".\n\n";
         exit(1);
      }

      for (int idx1=0; idx1<dim; idx1++) {
         data[idx1] = bad_data;
      }

      //
      // Retrieve the float value from the NetCDF variable.
      // Note: missing data was checked here
      //
      var->getVar(start, count, data);
      return_status = true;
   }
   return(return_status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data(NcVar *var, int *data, const long dim, const long cur) {
   return(_get_nc_data(var, data, bad_data_int, dim, cur));
}

////////////////////////////////////////////////////////////////////////

template <typename T>
bool _get_nc_data(NcVar *var, T *data, T bad_data, const long *dims, const long *curs) {
   bool return_status = false;
   const char *method_name = "_get_nc_data(const long *dims, const long *curs) ";

   if (IS_VALID_NC_P(var)) {
      std::vector<size_t> start;
      std::vector<size_t> count;

      int data_size = 1;
      int dimC = get_dim_count(var);
      for (int idx = 0 ; idx < dimC; idx++) {
         int dim_size = get_dim_size(var, idx);
         if ((curs[idx]+dims[idx]) > dim_size) {
            NcDim nc_dim = get_nc_dim(var, idx);
            mlog << Error << "\n" << method_name << "The start offset and count ("
                 << curs[idx] << ", " << dims[idx] << ") exceeds the dimension["
                 << idx << "] " << dim_size << " "
                 << (IS_VALID_NC(nc_dim) ? GET_NC_NAME(nc_dim) : " ")
                 << " for the variable " << GET_NC_NAME_P(var) << ".\n\n";
            exit(1);
         }

         start.push_back((size_t)curs[idx]);
         count.push_back((size_t)dims[idx]);
         data_size *= dims[idx];
      }

      for (int idx1=0; idx1<data_size; idx1++) {
         data[idx1] = bad_data;
      }

      //
      // Retrieve the float value from the NetCDF variable.
      // Note: missing data was checked here
      //
      var->getVar(start, count, data);
      return_status = true;
   }
   return(return_status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data(NcVar *var, int *data, const long *dims, const long *curs) {
   bool return_status = _get_nc_data(var, data, bad_data_int, dims, curs);

   return(return_status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data(NcVar *var, short *data, const long *curs) {
   bool return_status = _get_nc_data(var, data, (short)bad_data_int, curs);

   return(return_status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data(NcVar *var, short *data, const long *dims, const long *curs) {
   bool return_status = _get_nc_data(var, data, (short)bad_data_int, dims, curs);

   return(return_status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data(NcFile *nc, const char *var_name, float *data,
                 const long *dims, const long *curs) {

   //
   // Retrieve the input variables
   //
   NcVar var    = get_var(nc, var_name);
   return _get_nc_data(&var, data, bad_data_float, dims, curs);
}

////////////////////////////////////////////////////////////////////////

template <typename T>
void _apply_scale_factor(float *data, const T *packed_data,
                        const int cell_count, const T fill_value,
                        T &raw_min_val, T &raw_max_val, const char *data_type,
                        float add_offset, float scale_factor) {
   int positive_cnt = 0;
   int unpacked_count = 0;
   float min_value =  10e10;
   float max_value = -10e10;
   clock_t start_clock = clock();
   const char *method_name = "apply_scale_factor(float)";

   for (int idx=0; idx<cell_count; idx++) {
      if (fill_value == packed_data[idx])
         data[idx] = bad_data_float;
      else {
         if (raw_min_val > packed_data[idx]) raw_min_val = packed_data[idx];
         if (raw_max_val < packed_data[idx]) raw_max_val = packed_data[idx];
         data[idx] = ((float)packed_data[idx] * scale_factor) + add_offset;
         if (data[idx] > 0) positive_cnt++;
         if (min_value > data[idx]) min_value = data[idx];
         if (max_value < data[idx]) max_value = data[idx];
         if (!is_eq(0., add_offset) && !is_eq(1., scale_factor)) unpacked_count++;
      }
   }
   mlog << Debug(4) << method_name << " unpacked data: count="
        << unpacked_count << " out of " << cell_count
        << ". FillValue(" << data_type << ")=" << fill_value << "\n";
   mlog << Debug(4) << method_name << "data range [" << min_value << " - " << max_value
        << "] raw data: [" << raw_min_val << " - " << raw_max_val << "] Positive count: "
        << positive_cnt << "\n";
   mlog << Debug(7) << method_name << " took " 
        << (clock()-start_clock)/double(CLOCKS_PER_SEC) << " seconds\n";
   return;
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data(NcVar *var, float *data) {
   clock_t start_clock = clock();
   bool return_status = false;
   static const char *method_name = "get_nc_data(NcVar *, float *) ";

   if (IS_VALID_NC_P(var)) {
      //
      // Retrieve the float value from the NetCDF variable.
      // Note: missing data was checked here
      //
      int type_id = GET_NC_TYPE_ID_P(var);
      return_status = true;
      if (NcType::nc_FLOAT == type_id) {
         var->getVar(data);
      }
      else if (NcType::nc_DOUBLE == type_id) {
         int cell_count = 1;
         for (int idx=0; idx<var->getDimCount();idx++) {
            cell_count *= get_dim_size(var, idx);
         }
         double *double_data = new double[cell_count];
         var->getVar(double_data);
         for (int idx=0; idx<cell_count; idx++) {
            data[idx] = (float)double_data[idx];
         }
         delete [] double_data;
      }
      else {
         int cell_count = 1;
         for (int idx=0; idx<var->getDimCount();idx++) {
            cell_count *= get_dim_size(var, idx);
         }

         float add_offset = 0.;
         float scale_factor = 1.;
         int unpacked_count = 0;
         bool unsigned_value = has_unsigned_attribute(var);
         NcVarAtt *att_add_offset   = get_nc_att(var, string("add_offset"));
         NcVarAtt *att_scale_factor = get_nc_att(var, string("scale_factor"));
         NcVarAtt *att_fill_value   = get_nc_att(var, string("_FillValue"));
         if (IS_VALID_NC_P(att_add_offset)) add_offset = get_att_value_float(att_add_offset);
         if (IS_VALID_NC_P(att_scale_factor)) scale_factor = get_att_value_float(att_scale_factor);
         mlog << Debug(4) << method_name << "add_offset = " << add_offset
              << ", scale_factor=" << scale_factor << ", cell_count=" << cell_count
              << ", is_unsigned_value: " << unsigned_value << " for " << GET_NC_NAME_P(var) << "\n";

         switch ( type_id )  {
            case NcType::nc_INT64:
               {
                  long long fill_value = bad_data_int;
                  long long min_value =  2147483647;
                  long long max_value = -2147483648;
                  long long *packed_data = new long long[cell_count];

                  if (IS_VALID_NC_P(att_fill_value))
                     fill_value = get_att_value_int(att_fill_value);

                  var->getVar(packed_data);
                  _apply_scale_factor(data, packed_data, cell_count,
                                      fill_value, min_value, max_value, "int64",
                                      add_offset, scale_factor);
                  delete [] packed_data;
               }
               break;
            case NcType::nc_INT:
               {
                  int fill_value = bad_data_int;
                  int min_value =  2147483647;
                  int max_value = -2147483648;
                  int *packed_data = new int[cell_count];

                  if (IS_VALID_NC_P(att_fill_value))
                     fill_value = get_att_value_int(att_fill_value);

                  var->getVar(packed_data);
                  _apply_scale_factor(data, packed_data, cell_count,
                                      fill_value, min_value, max_value, "int",
                                      add_offset, scale_factor);
                  delete [] packed_data;
               }
               break;
            case NcType::nc_SHORT:
               {
                  short fill_value = (short)bad_data_int;
                  short *packed_data = new short[cell_count];

                  if (IS_VALID_NC_P(att_fill_value))
                     fill_value = get_att_value_short(att_fill_value);

                  var->getVar(packed_data);

                  if (unsigned_value) {
                     unsigned short value;
                     int positive_cnt = 0;
                     int raw_min_value =  70000;
                     int raw_max_value = -70000;
                     float min_value =  10e10;
                     float max_value = -10e10;
                     unsigned short unsigned_fill_value = (unsigned short)fill_value;
                     for (int idx=0; idx<cell_count; idx++) {
                        value = (unsigned short)packed_data[idx];
                        if (unsigned_fill_value == value)
                           data[idx] = bad_data_float;
                        else {
                           data[idx] = (value * scale_factor) + add_offset;
                           unpacked_count++;
                           if (raw_min_value > value) raw_min_value = value;
                           if (raw_max_value < value) raw_max_value = value;
                           if (data[idx] > 0) positive_cnt++;
                           if (min_value > data[idx]) min_value = data[idx];
                           if (max_value < data[idx]) max_value = data[idx];
                        }
                     }
                     mlog << Debug(4) << method_name << " unpacked data: count="
                          << unpacked_count << " out of " << cell_count
                          << ". FillValue(short with unsigned) " << fill_value
                          << " data range [" << min_value << " - " << max_value
                          << "] raw data: [" << raw_min_value << " - " << raw_max_value << "] Positive count: "
                          << positive_cnt << "\n";
                  }
                  else {
                     short min_value =  32766;
                     short max_value = -32767;
                     _apply_scale_factor(data, packed_data,
                           cell_count, fill_value, min_value, max_value, "short",
                           add_offset, scale_factor);
                  }
                  delete [] packed_data;
               }
               break;
            case NcType::nc_USHORT:
               {
                  unsigned short min_value = 65535;
                  unsigned short max_value = 0;
                  unsigned short fill_value = (unsigned short)bad_data_int;
                  unsigned short *packed_data = new unsigned short[cell_count];

                  if (IS_VALID_NC_P(att_fill_value))
                     fill_value = get_att_value_ushort(att_fill_value);

                  var->getVar(packed_data);

                  _apply_scale_factor(data, packed_data, cell_count,
                                      fill_value, min_value, max_value, "unsigned short",
                                      add_offset, scale_factor);
                  delete [] packed_data;
               }
               break;
            case NcType::nc_BYTE:
               {
                  ncbyte fill_value = (ncbyte)bad_data_int;
                  ncbyte *packed_data = new ncbyte[cell_count];

                  if (IS_VALID_NC_P(att_fill_value)) {
                     fill_value = get_att_value_char(att_fill_value);
                  }

                  var->getVar(packed_data);

                  if (unsigned_value) {
                     int value;
                     int positive_cnt = 0;
                     int raw_min_value =  70000;
                     int raw_max_value = -70000;
                     float min_value =  10e10;
                     float max_value = -10e10;
                     int unsigned_fill_value = (ncbyte)fill_value;
                     for (int idx=0; idx<cell_count; idx++) {
                        value = packed_data[idx];
                        if (unsigned_fill_value == value)
                           data[idx] = bad_data_float;
                        else {
                           data[idx] = (value * scale_factor) + add_offset;
                           unpacked_count++;
                           if (raw_min_value > value) raw_min_value = value;
                           if (raw_max_value < value) raw_max_value = value;
                           if (data[idx] > 0) positive_cnt++;
                           if (min_value > data[idx]) min_value = data[idx];
                           if (max_value < data[idx]) max_value = data[idx];
                        }
                     }
                     mlog << Debug(4) << method_name << " unpacked data: count="
                          << unpacked_count << " out of " << cell_count
                          << ". FillValue(byte with unsigned) " << fill_value
                          << " data range [" << min_value << " - " << max_value
                          << "] raw data: [" << raw_min_value << " - " << raw_max_value << "] Positive count: "
                          << positive_cnt << "\n";
                  }
                  else {
                     ncbyte min_value =  127;
                     ncbyte max_value = -127;
                     _apply_scale_factor(data, packed_data, cell_count,
                                         fill_value, min_value, max_value, "ncbyte",
                                         add_offset, scale_factor);
                  }
                  delete [] packed_data;
               }
               break;
            case NcType::nc_UBYTE:
               {
                  unsigned char min_value = 255;
                  unsigned char max_value = 0;
                  unsigned char fill_value = (unsigned char)-99;
                  unsigned char *packed_data = new unsigned char[cell_count];

                  if (IS_VALID_NC_P(att_fill_value)) {
                     fill_value = get_att_value_char(att_fill_value);
                  }

                  _apply_scale_factor(data, packed_data, cell_count,
                                      fill_value, min_value, max_value, "unsigned char",
                                      add_offset, scale_factor);
                  delete [] packed_data;
               }
               break;
            default:
               return_status = false;
               mlog << Debug(1) << method_name << "Did not read data because of unsupported data type ("
                    << type_id << ", type name: " << GET_NC_TYPE_NAME_P(var)
                    << ") for " << GET_NC_NAME_P(var) << "\n";
         }
         if(att_add_offset) delete att_add_offset;
         if(att_scale_factor) delete att_scale_factor;
         if(att_fill_value) delete att_fill_value;
      }
   }

   mlog << Debug(6) << method_name << "took "
        << (clock()-start_clock)/double(CLOCKS_PER_SEC)
        << " seconds for " << GET_NC_NAME_P(var) << "\n";
   return(return_status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data(NcVar *var, float *data, const long *curs) {
   bool return_status = _get_nc_data(var, data, bad_data_float, curs);

   return(return_status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data(NcVar *var, float *data, const long *dims, const long *curs) {
   bool return_status = _get_nc_data(var, data, bad_data_float, dims, curs);

   return(return_status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data(NcVar *var, float *data, const long dim, const long cur) {
   bool return_status = _get_nc_data(var, data, bad_data_float, dim, cur);

   return(return_status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data(NcFile *nc, const char *var_name, double *data,
                 const long *dims, const long *curs) {

   //
   // Retrieve the input variables
   //
   NcVar var    = get_var(nc, var_name);
   return get_nc_data(&var, data, dims, curs);
}

////////////////////////////////////////////////////////////////////////

template <typename T>
int _apply_scale_factor(double *data, const T *packed_data,
                        const int cell_count, const T fill_value,
                        T &raw_min_val, T &raw_max_val, const char *data_type,
                        double add_offset, double scale_factor) {
   int positive_cnt = 0;
   int unpacked_count = 0;
   double min_value =  10e10;
   double max_value = -10e10;
   const char *method_name = "apply_scale_factor(double)";

   for (int idx=0; idx<cell_count; idx++) {
      if (fill_value == packed_data[idx])
         data[idx] = bad_data_float;
      else {
         if (raw_min_val > packed_data[idx]) raw_min_val = packed_data[idx];
         if (raw_max_val < packed_data[idx]) raw_max_val = packed_data[idx];
         data[idx] = ((double)packed_data[idx] * scale_factor) + add_offset;
         if (data[idx] > 0) positive_cnt++;
         if (min_value > data[idx]) min_value = data[idx];
         if (max_value < data[idx]) max_value = data[idx];
         if (!is_eq(0., add_offset) && !is_eq(1., scale_factor)) unpacked_count++;
      }
   }
   mlog << Debug(4) << method_name << " unpacked data: count="
        << unpacked_count << " out of " << cell_count
        << ". FillValue(" << data_type << ")=" << fill_value
        << " data range [" << min_value << " - " << max_value
        << "] raw data: [" << raw_min_val << " - " << raw_max_val << "] Positive count: "
        << positive_cnt << "\n";
   return unpacked_count;
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data(NcVar *var, double *data) {
   bool return_status = false;
   static const char *method_name = "get_nc_data(NcVar *, double *) ";

   if (IS_VALID_NC_P(var)) {
      //
      // Retrieve the double value from the NetCDF variable.
      // Note: missing data was checked here
      //
      int unpacked_count = 0;
      int type_id = GET_NC_TYPE_ID_P(var);
      return_status = true;
      if ((NcType::nc_DOUBLE == type_id) || (NcType::nc_FLOAT == type_id)){
         var->getVar(data);
      }
      else {
         int cell_count = 1;
         for (int idx=0; idx<var->getDimCount();idx++) {
            cell_count *= get_dim_size(var, idx);
         }

         double add_offset = 0.;
         double scale_factor = 1.;
         bool unsigned_value = has_unsigned_attribute(var);
         NcVarAtt *att_add_offset   = get_nc_att(var, (string)"add_offset");
         NcVarAtt *att_scale_factor = get_nc_att(var, (string)"scale_factor");
         NcVarAtt *att_fill_value   = get_nc_att(var, (string)"_FillValue");
         if (IS_VALID_NC_P(att_add_offset)) {
            add_offset = get_att_value_double(att_add_offset);
         }
         if (IS_VALID_NC_P(att_scale_factor)) {
            scale_factor = get_att_value_double(att_scale_factor);
         }
         mlog << Debug(4) << method_name << "add_offset = " << add_offset
              << ", scale_factor=" << scale_factor << ", cell_count=" << cell_count
              << ", is_unsigned_value: " << unsigned_value << " for " << GET_NC_NAME_P(var) << "\n";

         switch ( type_id )  {
            case NcType::nc_INT64:
               {
                  long long fill_value = bad_data_int;
                  long long min_value =  2147483647;
                  long long max_value = -2147483648;
                  long long *packed_data = new long long[cell_count];

                  if (IS_VALID_NC_P(att_fill_value))
                     fill_value = get_att_value_int(att_fill_value);

                  var->getVar(packed_data);
                  _apply_scale_factor(data, packed_data, cell_count,
                                      fill_value, min_value, max_value, "int64",
                                      add_offset, scale_factor);
                  delete [] packed_data;
               }
               break;
            case NcType::nc_INT:
               {
                  int fill_value = bad_data_int;
                  int min_value =  2147483647;
                  int max_value = -2147483648;
                  int *packed_data = new int[cell_count];

                  if (IS_VALID_NC_P(att_fill_value))
                     fill_value = get_att_value_int(att_fill_value);

                  var->getVar(packed_data);
                  _apply_scale_factor(data, packed_data, cell_count,
                                      fill_value, min_value, max_value, "int",
                                      add_offset, scale_factor);
                  delete [] packed_data;
               }
               break;
            case NcType::nc_SHORT:
               {
                  short fill_value = (short)bad_data_int;
                  short *packed_data = new short[cell_count];

                  if (IS_VALID_NC_P(att_fill_value))
                     fill_value = get_att_value_short(att_fill_value);

                  var->getVar(packed_data);

                  if (unsigned_value) {
                     int value;
                     int positive_cnt = 0;
                     int raw_min_value =  70000;
                     int raw_max_value = -70000;
                     float min_value =  10e10;
                     float max_value = -10e10;
                     int unsigned_fill_value = (unsigned short)fill_value;
                     for (int idx=0; idx<cell_count; idx++) {
                        value = (unsigned short)packed_data[idx];
                        if (unsigned_fill_value == value)
                           data[idx] = bad_data_double;
                        else {
                           data[idx] = (value * scale_factor) + add_offset;
                           unpacked_count++;
                           if (raw_min_value > value) raw_min_value = value;
                           if (raw_max_value < value) raw_max_value = value;
                           if (data[idx] > 0) positive_cnt++;
                           if (min_value > data[idx]) min_value = data[idx];
                           if (max_value < data[idx]) max_value = data[idx];
                        }
                     }
                     mlog << Debug(4) << method_name << " unpacked data: count="
                          << unpacked_count << " out of " << cell_count
                          << ". FillValue(short with unsigned) " << fill_value
                          << " data range [" << min_value << " - " << max_value
                          << "] raw data: [" << raw_min_value << " - " << raw_max_value << "] Positive count: "
                          << positive_cnt << "\n";
                  }
                  else {
                     short min_value =  32766;
                     short max_value = -32767;
                     _apply_scale_factor(data, packed_data,
                           cell_count, fill_value, min_value, max_value, "int",
                           add_offset, scale_factor);
                  }
                  delete [] packed_data;
               }
               break;
            case NcType::nc_USHORT:
               {
                  unsigned short fill_value = (unsigned short)bad_data_int;
                  unsigned short *packed_data = new unsigned short[cell_count];

                  if (IS_VALID_NC_P(att_fill_value))
                     fill_value = get_att_value_short(att_fill_value);

                  var->getVar(packed_data);

                  unsigned short min_value = 65535;
                  unsigned short max_value = 0;
                  _apply_scale_factor(data, packed_data,
                        cell_count, fill_value, min_value, max_value, "int",
                        add_offset, scale_factor);
                  delete [] packed_data;
               }
               break;
            case NcType::nc_BYTE:
               {
                  ncbyte fill_value = (ncbyte)bad_data_int;
                  ncbyte *packed_data = new ncbyte[cell_count];

                  if (IS_VALID_NC_P(att_fill_value)) {
                     fill_value = get_att_value_char(att_fill_value);
                  }

                  var->getVar(packed_data);

                  if (unsigned_value) {
                     int value;
                     int positive_cnt = 0;
                     int raw_min_value =  70000;
                     int raw_max_value = -70000;
                     float min_value =  10e10;
                     float max_value = -10e10;
                     int unsigned_fill_value = (ncbyte)fill_value;
                     for (int idx=0; idx<cell_count; idx++) {
                        value = packed_data[idx];
                        if (unsigned_fill_value == value)
                           data[idx] = bad_data_double;
                        else {
                           data[idx] = (value * scale_factor) + add_offset;
                           unpacked_count++;
                           if (raw_min_value > value) raw_min_value = value;
                           if (raw_max_value < value) raw_max_value = value;
                           if (data[idx] > 0) positive_cnt++;
                           if (min_value > data[idx]) min_value = data[idx];
                           if (max_value < data[idx]) max_value = data[idx];
                        }
                     }
                     mlog << Debug(4) << method_name << " unpacked data: count="
                          << unpacked_count << " out of " << cell_count
                          << ". FillValue(short with unsigned) " << fill_value
                          << " data range [" << min_value << " - " << max_value
                          << "] raw data: [" << raw_min_value << " - " << raw_max_value << "] Positive count: "
                          << positive_cnt << "\n";
                  }
                  else {
                     ncbyte min_value =  127;
                     ncbyte max_value = -127;
                     _apply_scale_factor(data, packed_data, cell_count,
                                         fill_value, min_value, max_value, "ncbyte",
                                         add_offset, scale_factor);
                  }
                  delete [] packed_data;
               }
               break;
            case NcType::nc_UBYTE:
               {
                  signed char min_value = 255;
                  signed char max_value = 0;
                  signed char fill_value = (signed char)bad_data_int;
                  signed char *packed_data = new signed char[cell_count];

                  if (IS_VALID_NC_P(att_fill_value)) {
                     fill_value = get_att_value_char(att_fill_value);
                  }

                  var->getVar(packed_data);

                  _apply_scale_factor(data, packed_data, cell_count,
                                      fill_value, min_value, max_value, "ncbyte",
                                      add_offset, scale_factor);
                  delete [] packed_data;
               }
               break;
            default:
                 return_status = false;
                 mlog << Debug(1) << method_name << "Did not read data because of unsupported data type ("
                      << type_id << ", type name: " << GET_NC_TYPE_NAME_P(var)
                      << ") for " << GET_NC_NAME_P(var) << "\n";

         }
         if(att_add_offset) delete att_add_offset;
         if(att_scale_factor) delete att_scale_factor;
         if(att_fill_value) delete att_fill_value;
      }
   }
   return(return_status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data(NcVar *var, double *data, const long *curs) {
   bool return_status = _get_nc_data(var, data, bad_data_double, curs);
   return(return_status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data(NcVar *var, double *data, const long dim, const long cur) {
   bool return_status = _get_nc_data(var, data, bad_data_double, dim, cur);;

   return(return_status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data(NcVar *var, double *data, const long *dims, const long *curs) {
   bool return_status = _get_nc_data(var, data, bad_data_double, dims, curs);

   return(return_status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data(NcVar *var, char *data) {
   bool return_status = _get_nc_data(var, data);

   return(return_status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data(NcVar *var, uchar *data) {
   bool return_status = false;
   int data_type = GET_NC_TYPE_ID_P(var);
   static const char *method_name = "get_nc_data(NcVar *, uchar *) -> ";
   if (NC_UBYTE == data_type) return_status = _get_nc_data(var, data);
   else if (NC_BYTE == data_type && has_unsigned_attribute(var)) {
      int cell_count = 1;
      for (int idx=0; idx<var->getDimCount(); idx++) {
         cell_count *= get_dim_size(var, idx);
      }
      ncbyte *signed_data = new ncbyte[cell_count];
      return_status = _get_nc_data(var, signed_data);
      for (int idx=0; idx<cell_count; idx++) {
         data[idx] = (uchar)signed_data[idx];
      }
      delete [] signed_data;
   }
   else {
      mlog << Error << "\n" << method_name
           << "does not process \"" << GET_NC_TYPE_NAME_P(var)
           << "\" data type for variable \"" << GET_NC_NAME_P(var) << "\".\n\n";
      exit(1);
   }

   return(return_status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data(NcVar *var, unsigned short *data) {
   bool return_status = false;
   int data_type = GET_NC_TYPE_ID_P(var);
   static const char *method_name = "get_nc_data(NcVar *, unsigned short *) -> ";
   if (NC_USHORT == data_type) return_status = _get_nc_data(var, data);
   else if (NC_SHORT == data_type && has_unsigned_attribute(var)) {
      int cell_count = 1;
      short fill_value = (short)bad_data_int;
      NcVarAtt *att_fill_value  = get_nc_att(var, (string)"_FillValue");
      bool has_fill_value = IS_VALID_NC_P(att_fill_value);
      if (has_fill_value) fill_value = get_att_value_int(att_fill_value);
      for (int idx=0; idx<var->getDimCount(); idx++) {
         NcDim dim = var->getDim(idx);
         cell_count *= get_dim_size(&dim);
      }
      short *short_data = new short[cell_count];
      return_status = _get_nc_data(var, short_data);
      for (int idx=0; idx<cell_count; idx++) {
         if (has_fill_value && fill_value == short_data[idx])
            data[idx] = (unsigned short)bad_data_int;
         else
            data[idx] = (unsigned short)short_data[idx];
      }
      delete [] short_data;
      if (att_fill_value) delete att_fill_value;
   }
   else {
      mlog << Error << "\n" << method_name
           << "does not process \"" << GET_NC_TYPE_NAME_P(var)
           << "\" data type for variable \"" << GET_NC_NAME_P(var) << "\".\n\n";
      exit(1);
   }

   return(return_status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data(NcFile *nc, const char *var_name, char *data,
                 const long *dims, const long *curs) {

   //
   // Retrieve the input variables
   //
   NcVar var    = get_var(nc, var_name);
   return get_nc_data(&var, data, dims, curs);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data(NcVar *var, char *data, const long dim, const long cur) {
   bool return_status = _get_nc_data(var, data, bad_data_char, dim, cur);

   return(return_status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data(NcVar *var, char *data, const long *dims, const long *curs) {
   bool return_status = _get_nc_data(var, data, bad_data_char, dims, curs);

   return(return_status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data(NcVar *var, ncbyte *data) {
   bool return_status = _get_nc_data(var, data);

   return(return_status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data(NcFile *nc, const char *var_name, ncbyte *data,
                 const long *dims, const long *curs) {

   //
   // Retrieve the input variables
   //
   NcVar var = get_var(nc, var_name);
   return get_nc_data(&var, data, dims, curs);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data(NcVar *var, ncbyte *data, const long dim, const long cur) {
   bool return_status = _get_nc_data(var, data, (ncbyte)bad_data_char, dim, cur);

   return(return_status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data(NcVar *var, ncbyte *data, const long *dims, const long *curs) {
   bool return_status = _get_nc_data(var, data, (ncbyte)bad_data_char, dims, curs);

   return(return_status);
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data_to_array(NcVar *var, StringArray *array_buf) {
   bool result = false;
   static const char *method_name = "get_nc_data_to_array(NcVar) -> ";
   if (IS_INVALID_NC_P(var)) {
      mlog << Error << "\n" << method_name << "the variable does not exist!\n\n";
   }
   else {
      int dim_count = var->getDimCount();
      if (2 != dim_count) {
         mlog << Error << "\n" << method_name
              << "Invalid dimensions " <<  dim_count << " for "
              << GET_NC_NAME_P(var) << "\n\n";
      }
      else {
         long offsets[2] = { 0, 0 };
         long lengths[2] = { 1, 1 };
         NcDim count_dim = var->getDim(dim_count-2);
         NcDim str_dim = var->getDim(dim_count-1);
         int count = get_dim_size(&count_dim);
         int str_len = get_dim_size(&str_dim);
         lengths[1] = str_len;
         char str_buffer[str_len+1];
         result = true;
         for (int idx=0; idx<count; idx++) {
            if(!get_nc_data(var, str_buffer, lengths, offsets)) {
               result = false;
               break;
            }
            else {
               array_buf->add(str_buffer);
            }
            offsets[0]++;
         }
      }
   }
   return result;
}

////////////////////////////////////////////////////////////////////////

bool get_nc_data_to_array(NcFile *nc_in, const char *var_name,
                          StringArray *array_buf) {
   bool result = false;
   static const char *method_name = "get_nc_data_to_array(NcFile) -> ";
   NcVar obs_var = get_nc_var(nc_in, var_name);
   if (IS_INVALID_NC(obs_var)) {
      mlog << Error << "\n" << method_name << "the variable \"" << var_name
           << "\" does not exist!\n\n";
   }
   else {
      result = get_nc_data_to_array(&obs_var, array_buf);
   }
   return result;
}

///////////////////////////////////////////////////////////////////////////////

int get_nc_string_length(NcVar *var) {
   int str_length = 0;
   if (IS_VALID_NC_P(var)) {
      int dim_count = var->getDimCount();
      NcDim str_dim = var->getDim(dim_count-1);
      if (IS_VALID_NC(str_dim)) str_length = get_dim_size(&str_dim);
   }
   return str_length;
}

///////////////////////////////////////////////////////////////////////////////

int get_nc_string_length(NcFile *nc_in, const char *var_name) {
   int str_length = 0;
   NcVar obs_var = get_nc_var(nc_in, var_name);
   str_length = get_nc_string_length(&obs_var);
   return str_length;
}

///////////////////////////////////////////////////////////////////////////////

int get_nc_string_length(NcFile *nc_file, NcVar var, const char *var_name) {
   int string_len = IS_INVALID_NC(var)
                        ? get_nc_string_length(nc_file, var_name)
                        : get_nc_string_length(&var);
   return string_len;
}

////////////////////////////////////////////////////////////////////////

template <typename T>
bool _put_nc_data(NcVar *var, const T data, long offset0, long offset1, long offset2) {
   vector<size_t> offsets;
   offsets.push_back((size_t)offset0);
   if (0 <= offset1) {
     offsets.push_back((size_t)offset1);
   }
   if (0 <= offset2) {
     offsets.push_back((size_t)offset2);
   }
   var->putVar(offsets, data);
   return true;
}

////////////////////////////////////////////////////////////////////////

bool put_nc_data(NcVar *var, const int data, long offset0, long offset1, long offset2) {
   return _put_nc_data(var, data, offset0, offset1, offset2);
}

////////////////////////////////////////////////////////////////////////

bool put_nc_data(NcVar *var, const char data, long offset0, long offset1, long offset2) {
   return _put_nc_data(var, data, offset0, offset1, offset2);
}

////////////////////////////////////////////////////////////////////////

bool put_nc_data(NcVar *var, const float data , long offset0, long offset1, long offset2) {
   return _put_nc_data(var, data, offset0, offset1, offset2);
}

////////////////////////////////////////////////////////////////////////

bool put_nc_data(NcVar *var, const double data, long offset0, long offset1, long offset2) {
   return _put_nc_data(var, data, offset0, offset1, offset2);
}

////////////////////////////////////////////////////////////////////////

bool put_nc_data(NcVar *var, const ncbyte data, long offset0, long offset1, long offset2) {
   return _put_nc_data(var, data, offset0, offset1, offset2);
}

////////////////////////////////////////////////////////////////////////

bool put_nc_data(NcVar *var, const int *data    ) {
   var->putVar(data);
   return true;
}

////////////////////////////////////////////////////////////////////////

bool put_nc_data(NcVar *var, const char *data   ) {
   var->putVar(data);
   return true;
}

////////////////////////////////////////////////////////////////////////

bool put_nc_data(NcVar *var, const float *data  ) {
   var->putVar(data);
   return true;
}

////////////////////////////////////////////////////////////////////////

bool put_nc_data(NcVar *var, const double *data ) {
   var->putVar(data);
   return true;
}

////////////////////////////////////////////////////////////////////////

bool put_nc_data(NcVar *var, const ncbyte *data ) {
   var->putVar(data);
   return true;
}

////////////////////////////////////////////////////////////////////////

template <typename T>
bool _put_nc_data(NcVar *var, const T *data,    const long length, const long offset) {
   vector<size_t> offsets, counts;
   int dim_count = get_dim_count(var);
   offsets.push_back(offset);
   if (dim_count >= 2) {
      offsets.push_back(0);
      counts.push_back(1);
   }
   counts.push_back(length);
   var->putVar(offsets, counts, data);
   return true;
}

////////////////////////////////////////////////////////////////////////

bool put_nc_data(NcVar *var, const int *data,    const long length, const long offset) {
   _put_nc_data(var, data, length, offset);
   return true;
}

////////////////////////////////////////////////////////////////////////

bool put_nc_data(NcVar *var, const char *data,   const long length, const long offset) {
   _put_nc_data(var, data, length, offset);
   return true;
}

////////////////////////////////////////////////////////////////////////

bool put_nc_data(NcVar *var, const float *data , const long length, const long offset) {
   _put_nc_data(var, data, length, offset);
   return true;
}

////////////////////////////////////////////////////////////////////////

bool put_nc_data(NcVar *var, const double *data, const long length, const long offset) {
   _put_nc_data(var, data, length, offset);
   return true;
}

////////////////////////////////////////////////////////////////////////

bool put_nc_data(NcVar *var, const ncbyte *data, const long length, const long offset) {
   _put_nc_data(var, data, length, offset);
   return true;
}

////////////////////////////////////////////////////////////////////////

template <typename T>
bool _put_nc_data(NcVar *var, const T *data , const long *lengths, const long *offsets) {
   int dim = get_dim_count(var);
   vector<size_t> nc_offsets, counts;
   for (int idx = 0 ; idx < dim; idx++) {
      nc_offsets.push_back(offsets[idx]);
   }
   for (int idx = 0 ; idx < dim; idx++) {
      counts.push_back(lengths[idx]);
   }
   var->putVar(nc_offsets, counts, data);
   return true;
}

////////////////////////////////////////////////////////////////////////

bool put_nc_data(NcVar *var, const float *data , const long *lengths, const long *offsets) {
   _put_nc_data(var, data , lengths, offsets);
   return true;
}

////////////////////////////////////////////////////////////////////////

bool put_nc_data(NcVar *var, const char *data , const long *lengths, const long *offsets) {
   _put_nc_data(var, data , lengths, offsets);
   return true;
}

////////////////////////////////////////////////////////////////////////

bool put_nc_data(NcVar *var, const int *data , const long *lengths, const long *offsets) {
   _put_nc_data(var, data , lengths, offsets);
   return true;
}

////////////////////////////////////////////////////////////////////////

template <typename T>
bool _put_nc_data_with_dims(NcVar *var, const T *data,
                           const long len0, const long len1, const long len2) {
   vector<size_t> offsets, counts;
   if (0 < len0) {
     offsets.push_back(0);
     counts.push_back(len0);
   }
   if (0 < len1) {
     offsets.push_back(0);
     counts.push_back(len1);
   }
   if (0 < len2) {
     offsets.push_back(0);
     counts.push_back(len2);
   }
   var->putVar(offsets, counts, data);
   return true;
}

////////////////////////////////////////////////////////////////////////

bool put_nc_data_with_dims(NcVar *var, const int *data,
                           const int len0, const int len1, const int len2) {
   return put_nc_data_with_dims(var, data, (long)len0, (long)len1, (long)len2);
}

////////////////////////////////////////////////////////////////////////

bool put_nc_data_with_dims(NcVar *var, const int *data,
                           const long len0, const long len1, const long len2) {
   _put_nc_data_with_dims(var, data, len0, len1, len2);
   return true;
}

////////////////////////////////////////////////////////////////////////

bool put_nc_data_with_dims(NcVar *var, const float *data,
                           const int len0, const int len1, const int len2) {
   return put_nc_data_with_dims(var, data, (long)len0, (long)len1, (long)len2);
}

////////////////////////////////////////////////////////////////////////

bool put_nc_data_with_dims(NcVar *var, const float *data,
                           const long len0, const long len1, const long len2) {
   _put_nc_data_with_dims(var, data, len0, len1, len2);
   return true;
}

////////////////////////////////////////////////////////////////////////

bool put_nc_data_with_dims(NcVar *var, const double *data,
                           const int len0, const int len1, const int len2) {
   return put_nc_data_with_dims(var, data, (long)len0, (long)len1, (long)len2);
}

////////////////////////////////////////////////////////////////////////

bool put_nc_data_with_dims(NcVar *var, const double *data,
                           const long len0, const long len1, const long len2) {
   _put_nc_data_with_dims(var, data, len0, len1, len2);
   return true;
}

////////////////////////////////////////////////////////////////////////

bool args_ok(const LongArray & a) {
   int j, k;

   for (j=0; j<(a.n_elements()); ++j)  {

      k = a[j];

      if ( (k < 0) && (k != vx_data2d_star) ) return (false);
   }

   return(true);
}

////////////////////////////////////////////////////////////////////////

NcVar get_var(NcFile *nc, const char *var_name) {
   string new_var_name = var_name;
   patch_nc_name(&new_var_name);

   //
   // Retrieve the variable from the NetCDF file.
   //
   NcVar var;
   multimap<string,NcVar> varMap = GET_NC_VARS_P(nc);
   multimap<string,NcVar>::iterator it = varMap.find(new_var_name);
   if (it != varMap.end()) {
      NcVar tmpVar = it->second;
      if(IS_INVALID_NC(tmpVar)) {
         mlog << Error << "\nget_var() -> "
              << "can't read \"" << new_var_name << "\" variable.\n\n";
         exit(1);
      }

      var = tmpVar;
   }

   return(var);
}

////////////////////////////////////////////////////////////////////////

NcVar get_nc_var(NcFile *nc, const char *var_name, bool log_as_error) {
   string new_var_name = var_name;
   patch_nc_name(&new_var_name);

   //
   // Retrieve the variable from the NetCDF file.
   //
   NcVar var = nc->getVar(new_var_name);
   if(IS_INVALID_NC(var)) {
      ConcatString log_message;
      log_message << "\nget_nc_var(NcFile) --> The variable \""
                  << new_var_name << "\" does not exist!\n\n";
      if (log_as_error)
         mlog << Error << log_message;
      else
         mlog << Warning << log_message;
   }

   return(var);
}

////////////////////////////////////////////////////////////////////////

void copy_nc_att_byte(NcFile *nc_to, NcGroupAtt *from_att) {
   size_t att_length = from_att->getAttLength();
   ncbyte value[att_length];
   from_att->getValues((void *)&value);
   nc_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), att_length, (void *)value);
}

void copy_nc_att_char(NcFile *nc_to, NcGroupAtt *from_att) {
   size_t att_length = from_att->getAttLength();
   char value[att_length];
   from_att->getValues((void *)&value);
   nc_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), att_length, (void *)value);
}

void copy_nc_att_double(NcFile *nc_to, NcGroupAtt *from_att) {
   size_t att_length = from_att->getAttLength();
   if (att_length == 1) {
      double value;
      from_att->getValues(&value);
      nc_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), value);
   }
   else {
      double values[att_length];
      from_att->getValues(&values);
      nc_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), att_length, values);
   }
}

void copy_nc_att_float(NcFile *nc_to, NcGroupAtt *from_att) {
   size_t att_length = from_att->getAttLength();
   if (att_length == 1) {
      float value;
      from_att->getValues(&value);
      nc_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), value);
   }
   else {
      float values[att_length];
      from_att->getValues(&values);
      nc_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), att_length, values);
   }
}

void copy_nc_att_int(NcFile *nc_to, NcGroupAtt *from_att) {
   size_t att_length = from_att->getAttLength();
   if (att_length == 1) {
      int value;
      from_att->getValues(&value);
      nc_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), value);
   }
   else {
      int values[att_length];
      from_att->getValues(&values);
      nc_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), att_length, values);
   }
}

void copy_nc_att_int64(NcFile *nc_to, NcGroupAtt *from_att) {
   size_t att_length = from_att->getAttLength();
   if (att_length == 1) {
      long long value;
      from_att->getValues(&value);
      nc_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), value);
   }
   else {
      long long values[att_length];
      from_att->getValues(&values);
      nc_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), att_length, values);
   }
}

void copy_nc_att_short(NcFile *nc_to, NcGroupAtt *from_att) {
   size_t att_length = from_att->getAttLength();
   if (att_length == 1) {
      short value;
      from_att->getValues(&value);
      nc_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), value);
   }
   else {
      short values[att_length];
      from_att->getValues(&values);
      nc_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), att_length, values);
   }
}

////////////////////////////////////////////////////////////////////////

void copy_nc_att_char(NcVar *var_to, NcGroupAtt *from_att) {
   size_t att_length = from_att->getAttLength();
   char value[att_length];
   from_att->getValues((void *)&value);
   var_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), att_length, (void *)value);
}

void copy_nc_att_double(NcVar *var_to, NcGroupAtt *from_att) {
   size_t att_length = from_att->getAttLength();
   if (att_length == 1) {
      double value;
      from_att->getValues(&value);
      var_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), value);
   }
   else {
      double values[att_length];
      from_att->getValues(&values);
      var_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), att_length, values);
   }
}
void copy_nc_att_float(NcVar *var_to, NcGroupAtt *from_att) {
   size_t att_length = from_att->getAttLength();
   if (att_length == 1) {
      float value;
      from_att->getValues(&value);
      var_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), value);
   }
   else {
      float values[att_length];
      from_att->getValues(&values);
      var_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), att_length, values);
   }
}

void copy_nc_att_int(NcVar *var_to, NcGroupAtt *from_att) {
   size_t att_length = from_att->getAttLength();
   if (att_length == 1) {
      int value;
      from_att->getValues(&value);
      var_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), value);
   }
   else {
      int values[att_length];
      from_att->getValues(&values);
      var_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), att_length, values);
   }
}

void copy_nc_att_int64(NcVar *var_to, NcGroupAtt *from_att) {
   size_t att_length = from_att->getAttLength();
   if (att_length == 1) {
      long long value;
      from_att->getValues(&value);
      var_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), value);
   }
   else {
      long long values[att_length];
      from_att->getValues(&values);
      var_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), att_length, values);
   }
}

void copy_nc_att_short(NcVar *var_to, NcGroupAtt *from_att) {
   size_t att_length = from_att->getAttLength();
   if (att_length == 1) {
      short value;
      from_att->getValues(&value);
      var_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), value);
   }
   else {
      short values[att_length];
      from_att->getValues(&values);
      var_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), att_length, values);
   }
}

////////////////////////////////////////////////////////////////////////

void copy_nc_att_byte(NcVar *var_to, NcVarAtt *from_att) {
   size_t att_length = from_att->getAttLength();
   ncbyte value[att_length];
   from_att->getValues((void *)&value);
   var_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), att_length, (void *)value);
}

void copy_nc_att_char(NcVar *var_to, NcVarAtt *from_att) {
   size_t att_length = from_att->getAttLength();
   char value[att_length];
   from_att->getValues((void *)&value);
   var_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), att_length, (void *)value);
}

void copy_nc_att_double(NcVar *var_to, NcVarAtt *from_att) {
   size_t att_length = from_att->getAttLength();
   if (att_length == 1) {
      double value;
      from_att->getValues(&value);
      var_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), value);
   }
   else {
      double values[att_length];
      from_att->getValues(&values);
      var_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), att_length, values);
   }
}

void copy_nc_att_float(NcVar *var_to, NcVarAtt *from_att) {
   size_t att_length = from_att->getAttLength();
   if (att_length == 1) {
      float value;
      from_att->getValues(&value);
      var_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), value);
   }
   else {
      float values[att_length];
      from_att->getValues(&values);
      var_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), att_length, values);
   }
}

void copy_nc_att_int(NcVar *var_to, NcVarAtt *from_att) {
   size_t att_length = from_att->getAttLength();
   if (att_length == 1) {
      int value;
      from_att->getValues(&value);
      var_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), value);
   }
   else {
      int values[att_length];
      from_att->getValues(&values);
      var_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), att_length, values);
   }
}

void copy_nc_att_int64(NcVar *var_to, NcVarAtt *from_att) {
   size_t att_length = from_att->getAttLength();
   if (att_length == 1) {
      long long value;
      from_att->getValues(&value);
      var_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), value);
   }
   else {
      long long values[att_length];
      from_att->getValues(&values);
      var_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), att_length, values);
   }
}

void copy_nc_att_short(NcVar *var_to, NcVarAtt *from_att) {
   size_t att_length = from_att->getAttLength();
   if (att_length == 1) {
      short value;
      from_att->getValues(&value);
      var_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), value);
   }
   else {
      short values[att_length];
      from_att->getValues(&values);
      var_to->putAtt(GET_NC_NAME_P(from_att), from_att->getType(), att_length, values);
   }
}


NcVar *copy_nc_var(NcFile *to_nc, NcVar *from_var,
      const int deflate_level, const bool all_attrs) {
   vector<NcDim> dims = from_var->getDims();
   for(unsigned int idx=0; idx<dims.size(); idx++) {
      NcDim dim = dims[idx];
      if (!has_dim(to_nc, GET_NC_NAME(dim).c_str())) {
         add_dim(to_nc, GET_NC_NAME(dim), dim.getSize());
      }
   }
   NcVar tmp_var = add_var(to_nc, GET_NC_NAME_P(from_var),
         from_var->getType(), dims, deflate_level);
   NcVar *to_var = new NcVar(tmp_var);
   copy_nc_atts(from_var, to_var, all_attrs);
   copy_nc_var_data(from_var, to_var);
   return to_var;
}

////////////////////////////////////////////////////////////////////////

void copy_nc_att(NcFile *nc_from, NcVar *var_to, const ConcatString attr_name) {
   NcGroupAtt *from_att = get_nc_att(nc_from, attr_name);
   if (IS_VALID_NC_P(from_att)) {
      int dataType = GET_NC_TYPE_ID_P(from_att);
      switch (dataType) {
      case NC_DOUBLE:
         copy_nc_att_double(var_to, from_att);
         break;
      case NC_FLOAT:
         copy_nc_att_float(var_to, from_att);
         break;
      case NC_SHORT:
         copy_nc_att_short(var_to, from_att);
         break;
      case NC_INT:
         copy_nc_att_int(var_to, from_att);
         break;
      case NC_INT64:
         copy_nc_att_int64(var_to, from_att);
         break;
      case NC_CHAR:
         copy_nc_att_char(var_to, from_att);
         break;
      default:
         mlog << Error << "\ncopy_nc_att(NcFile, NcVar, attr_name) -> "
              << "Does not copy this type \"" << dataType << "\" global NetCDF attribute.\n\n";
         exit(1);
      }
   }
   if(from_att) delete from_att;
}

////////////////////////////////////////////////////////////////////////

void copy_nc_att(NcVar *var_from, NcVar *var_to, const ConcatString attr_name) {
   NcVarAtt *from_att = get_nc_att(var_from, attr_name);
   if (IS_VALID_NC_P(from_att)) {
      int dataType = GET_NC_TYPE_ID_P(from_att);
      switch (dataType) {
      case NC_DOUBLE:
         copy_nc_att_double(var_to, from_att);
         break;
      case NC_FLOAT:
         copy_nc_att_float(var_to, from_att);
         break;
      case NC_SHORT:
         copy_nc_att_short(var_to, from_att);
         break;
      case NC_INT:
         copy_nc_att_int(var_to, from_att);
         break;
      case NC_INT64:
         copy_nc_att_int64(var_to, from_att);
         break;
      case NC_CHAR:
         copy_nc_att_char(var_to, from_att);
         break;
      default:
         mlog << Error << "\ncopy_nc_att(NcVar) -> "
              << "Does not copy this type \"" << dataType << "\" NetCDF attributes from \""
              << GET_NC_TYPE_NAME_P(var_from) << "\".\n\n";
         exit(1);
      }
   }
   if(from_att) delete from_att;
}

////////////////////////////////////////////////////////////////////////

void copy_nc_atts(NcFile *nc_from, NcFile *nc_to, const bool all_attrs) {
   multimap<string,NcGroupAtt> ncAttMap = nc_from->getAtts();
   for (multimap<string,NcGroupAtt>::iterator itr = ncAttMap.begin();
         itr != ncAttMap.end(); ++itr) {
      if (all_attrs ||
            (  (itr->first != "Conventions")
            && (itr->first != "missing_value") ) ) {
         NcGroupAtt *from_att = &(itr->second);
         int dataType = GET_NC_TYPE_ID_P(from_att);
         switch (dataType) {
         case NC_DOUBLE:
            copy_nc_att_double(nc_to, from_att);
            break;
         case NC_FLOAT:
            copy_nc_att_float(nc_to, from_att);
            break;
         case NC_SHORT:
            copy_nc_att_short(nc_to, from_att);
            break;
         case NC_INT:
            copy_nc_att_int(nc_to, from_att);
            break;
         case NC_INT64:
            copy_nc_att_int64(nc_to, from_att);
            break;
         case NC_CHAR:
            copy_nc_att_char(nc_to, from_att);
            break;
         default:
            mlog << Error << "\ncopy_nc_atts(NcFile) -> "
                 << "Does not copy this type \"" << dataType << "\" global NetCDF attributes.\n\n";
            exit(1);
         }
      }
   }
}

////////////////////////////////////////////////////////////////////////

void copy_nc_atts(NcVar *var_from, NcVar *var_to, const bool all_attrs) {
   std::map<std::string,NcVarAtt> ncAttMap = var_from->getAtts();
   for (std::map<std::string,NcVarAtt>::iterator itr = ncAttMap.begin();
         itr != ncAttMap.end(); ++itr) {
      if (all_attrs ||
            (  (itr->first != "scale_factor")
            && (itr->first != "add_offset")
            && (itr->first != "_FillValue")
            && (itr->first != "_Unsigned")
            && (itr->first != "valid_range")
            && (itr->first != "missing_value")
            && (itr->first != "grid_mapping")
            && (itr->first != "coordinates")
            && (itr->first != "cell_methods")
            && (itr->first != "_Com") ) ) {
         NcVarAtt *from_att = &(itr->second);
         int dataType = GET_NC_TYPE_ID_P(from_att);
         switch (dataType) {
         case NC_DOUBLE:
            copy_nc_att_double(var_to, from_att);
            break;
         case NC_FLOAT:
            copy_nc_att_float(var_to, from_att);
            break;
         case NC_SHORT:
            copy_nc_att_short(var_to, from_att);
            break;
         case NC_INT:
            copy_nc_att_int(var_to, from_att);
            break;
         case NC_INT64:
            copy_nc_att_int64(var_to, from_att);
            break;
         case NC_CHAR:
            copy_nc_att_char(var_to, from_att);
            break;
         case NC_BYTE:
            copy_nc_att_byte(var_to, from_att);
            break;
         default:
            mlog << Error << "\ncopy_nc_atts(NcVar) -> "
                 << "Does not copy this type \"" << dataType << "\" NetCDF attributes from \""
                 << GET_NC_TYPE_NAME_P(var_from) << "\".\n\n";
            exit(1);
         }
      }
   }
}

////////////////////////////////////////////////////////////////////////

void copy_nc_data_char(NcVar *var_from, NcVar *var_to, int data_size) {
   //const string method_name = "copy_nc_data_char";
   char *data = new char[data_size];
   var_from->getVar(data);
   var_to->putVar(data);
   //   mlog << Error << "\n" << method_name << " -> error writing the variable "
   //        << GET_NC_NAME_P(var_to) << " to the netCDF file\n\n";
   //   exit(1);
   delete[] data;
}

////////////////////////////////////////////////////////////////////////

void copy_nc_data_double(NcVar *var_from, NcVar *var_to, int data_size) {
   //const string method_name = "copy_nc_data_double";
   double *data = new double[data_size];
   var_from->getVar(data);
   var_to->putVar(data);
   //   mlog << Error << "\n" << method_name << " -> error writing the variable "
   //        << GET_NC_NAME_P(var_to) << " to the netCDF file\n\n";
   //   exit(1);
   delete[] data;
}

////////////////////////////////////////////////////////////////////////

void copy_nc_data_float(NcVar *var_from, NcVar *var_to, int data_size) {
   //const string method_name = "copy_nc_data_float";
   float *data = new float[data_size];
   var_from->getVar(data);
   var_to->putVar(data);
   //   mlog << Error << "\n" << method_name << " -> error writing the variable "
   //        << GET_NC_NAME_P(var_to) << " to the netCDF file\n\n";
   //   exit(1);
   delete[] data;
}

////////////////////////////////////////////////////////////////////////

void copy_nc_data_int(NcVar *var_from, NcVar *var_to, int data_size) {
   //const string method_name = "copy_nc_data_int";
   int *data = new int[data_size];
   var_from->getVar(data);
   var_to->putVar(data);
   //   mlog << Error << "\n" << method_name << " -> error writing the variable "
   //        << GET_NC_NAME_P(var_to) << " to the netCDF file\n\n";
   //   exit(1);
   delete[] data;
}

////////////////////////////////////////////////////////////////////////

void copy_nc_data_short(NcVar *var_from, NcVar *var_to, int data_size) {
   const string method_name = "copy_nc_data_double";
   short *data = new short[data_size];
   var_from->getVar(data);
   var_to->putVar(data);
   //   mlog << Error << "\n" << method_name << " -> error writing the variable "
   //        << GET_NC_NAME_P(var_to) << " to the netCDF file\n\n";
   //   exit(1);
   delete[] data;
}


void copy_nc_var_data(NcVar *var_from, NcVar *var_to) {
   const string method_name = "copy_nc_var_data()";
   int data_size = get_data_size(var_from);
   int dataType = GET_NC_TYPE_ID_P(var_from);
   switch (dataType) {
   case NC_DOUBLE:
      copy_nc_data_double(var_from, var_to, data_size);
      break;

   case NC_FLOAT:
      copy_nc_data_float(var_from, var_to, data_size);
      break;
   case NC_SHORT:
      copy_nc_data_short(var_from, var_to, data_size);
      break;
   case NC_INT:
      copy_nc_data_int(var_from, var_to, data_size);
      break;

   case NC_CHAR:
      copy_nc_data_char(var_from, var_to, data_size);
      break;

   default:
      mlog << Error << "\n" << method_name << " -> "
           << "Does not copy this type \"" << dataType << "\" NetCDF data from \""
           << GET_NC_TYPE_NAME_P(var_from) << "\".\n\n";
      exit(1);
   }
}

////////////////////////////////////////////////////////////////////////

void copy_nc_var_dims(NcVar *var_from, NcVar *var_to) {
   int dim_count = var_from->getDimCount();
   for (int idx=0; idx<dim_count; idx++) {
      NcDim fromDim = var_from->getDim(idx);
      GET_NC_NAME(fromDim);
   }
}

////////////////////////////////////////////////////////////////////////

bool has_var(NcFile *nc, const char * var_name) {
   NcVar v = get_var(nc, var_name);
   return IS_VALID_NC(v);
}

////////////////////////////////////////////////////////////////////////

NcVar add_var(NcFile *nc, const string &var_name, const NcType ncType, const int deflate_level) {
   std::vector<NcDim> ncDimVector;
   string new_var_name = var_name;
   patch_nc_name(&new_var_name);
   NcVar var = nc->addVar(new_var_name, ncType, ncDimVector);

   if (deflate_level > 0) {
      mlog << Debug(3) << "    nc_utils.add_var() deflate_level: " << deflate_level << "\n";
      var.setCompression(false, true, deflate_level);
   }
   return var;
}

////////////////////////////////////////////////////////////////////////

NcVar add_var(NcFile *nc, const string &var_name, const NcType ncType,
              const NcDim ncDim, const int deflate_level) {
   string new_var_name = var_name;
   patch_nc_name(&new_var_name);
   NcVar var = nc->addVar(new_var_name, ncType, ncDim);

   if (deflate_level > 0) {
      mlog << Debug(3) << "    nc_utils.add_var() deflate_level: " << deflate_level << "\n";
      var.setCompression(false, true, deflate_level);
   }
   return var;
}

////////////////////////////////////////////////////////////////////////

NcVar add_var(NcFile *nc, const string &var_name, const NcType ncType,
              const NcDim ncDim1, const NcDim ncDim2, const int deflate_level) {
   vector<NcDim> ncDims;
   ncDims.push_back(ncDim1);
   ncDims.push_back(ncDim2);
   return add_var(nc, var_name, ncType, ncDims, deflate_level);
}

////////////////////////////////////////////////////////////////////////

NcVar add_var(NcFile *nc, const string &var_name, const NcType ncType,
              const NcDim ncDim1, const NcDim ncDim2, const NcDim ncDim3, const int deflate_level) {
   vector<NcDim> ncDims;
   ncDims.push_back(ncDim1);
   ncDims.push_back(ncDim2);
   ncDims.push_back(ncDim3);
   return add_var(nc, var_name, ncType, ncDims, deflate_level);
}

////////////////////////////////////////////////////////////////////////

NcVar add_var(NcFile *nc, const string &var_name, const NcType ncType,
              const NcDim ncDim1, const NcDim ncDim2, const NcDim ncDim3,
              const NcDim ncDim4, const int deflate_level) {
   vector<NcDim> ncDims;
   ncDims.push_back(ncDim1);
   ncDims.push_back(ncDim2);
   ncDims.push_back(ncDim3);
   ncDims.push_back(ncDim4);
   return add_var(nc, var_name, ncType, ncDims, deflate_level);
}

////////////////////////////////////////////////////////////////////////

NcVar add_var(NcFile *nc, const string &var_name, const NcType ncType,
              const vector<NcDim> ncDims, const int deflate_level) {
   string new_var_name = var_name;
   patch_nc_name(&new_var_name);
   NcVar var = nc->addVar(new_var_name, ncType, ncDims);
   if (deflate_level > 0) {
      mlog << Debug(3) << "    nc_utils.add_var() deflate_level: " << deflate_level << "\n";
      var.setCompression(false, true, deflate_level);
   }
   return var;
}

////////////////////////////////////////////////////////////////////////

NcDim add_dim(NcFile *nc, const string &dim_name) {
   string new_dim_name = dim_name;
   patch_nc_name(&new_dim_name);
   return nc->addDim(new_dim_name);
}

////////////////////////////////////////////////////////////////////////

NcDim add_dim(NcFile *nc, const string &dim_name, const size_t dim_size) {
   string new_dim_name = dim_name;
   patch_nc_name(&new_dim_name);
   return nc->addDim(new_dim_name, dim_size);
}

////////////////////////////////////////////////////////////////////////

bool has_dim(NcFile *nc, const char * dim_name) {
   NcDim d = nc->getDim(dim_name);
   return IS_VALID_NC(d);
}

////////////////////////////////////////////////////////////////////////

bool get_dim(const NcFile *nc, const ConcatString &dim_name,
             int &dim_val, bool error_out) {
   NcDim dim;
   bool status = false;

   // Initialize
   dim_val = bad_data_int;

   dim = nc->getDim((string)dim_name);

   if(IS_VALID_NC(dim)) {
      dim_val = (int) (dim.getSize());
      status = true;
   }

   // Check error_out status
   if(error_out && !status) {
      mlog << Error << "\nget_dim() -> "
           << "can't find NetCDF dimension \"" << dim_name << "\".\n\n";
      exit(1);
   }

   return(status);
}

////////////////////////////////////////////////////////////////////////

int get_dim_count(const NcFile *nc) {
   return( IS_INVALID_NC_P(nc) ? -1 : nc->getDimCount());
}

////////////////////////////////////////////////////////////////////////

int get_dim_count(const NcVar *var) {
   return( IS_INVALID_NC_P(var) ? -1 : var->getDimCount());
}

////////////////////////////////////////////////////////////////////////

int get_dim_size(const NcDim *dim) {

   return( IS_INVALID_NC_P(dim) ? -1 : dim->getSize() );
}

////////////////////////////////////////////////////////////////////////

int get_dim_size(const NcVar *var, const int dim_offset) {
   int dim_size = -1;
   if(IS_VALID_NC_P(var)) {
      NcDim nc_dim = get_nc_dim(var, dim_offset);
      if (IS_VALID_NC(nc_dim)) dim_size = nc_dim.getSize();
   }

   return( dim_size );
}

////////////////////////////////////////////////////////////////////////

int get_dim_value(const NcFile *nc, const string &dim_name, const bool error_out) {
   NcDim dim;
   int dim_val;
   bool status = false;

   // Initialize
   dim_val = bad_data_int;

   dim = nc->getDim((string)dim_name);

   if(IS_VALID_NC(dim)) {
      dim_val = (int) (dim.getSize());
      status = true;
   }

   // Check error_out status
   if(error_out && !status) {
      mlog << Error << "\nget_dim() -> "
           << "can't find NetCDF dimension \"" << dim_name << "\".\n\n";
      exit(1);
   }

   return(dim_val);
}


////////////////////////////////////////////////////////////////////////

NcDim get_nc_dim(const NcFile *nc, const string &dim_name) {
   return nc->getDim(dim_name);
}

////////////////////////////////////////////////////////////////////////

NcDim get_nc_dim(const NcVar *var, const string &dim_name) {
   NcDim d;
   int dimCount = var->getDimCount();
   for (int idx=0; idx<dimCount; idx++) {
      NcDim dim = var->getDim(idx);
      if (strcmp(dim.getName().c_str(), dim_name.c_str()) == 0) {
         d = dim;
         break;
      }
   }
   return d;
}

////////////////////////////////////////////////////////////////////////

NcDim get_nc_dim(const NcVar *var, const int dim_offset) {
   if (var->getDimCount() > dim_offset)
      return var->getDim(dim_offset);
   else {
      NcDim d;
      return d;
   }
}


////////////////////////////////////////////////////////////////////////

bool get_dim_names(const NcFile *nc, StringArray *dimNames) {

   int i, dimCount;
   //NcDim dim;
   bool status = false;

   dimCount = nc->getDimCount();

   i = 0;
   multimap<string, NcDim>::iterator itDim;
   multimap<string, NcDim> dims = nc->getDims();
   for (itDim = dims.begin(); itDim != dims.end(); ++itDim) {
      dimNames->add((*itDim).first);
      i++;
   }

   if (i != dimCount) {
      mlog << Error << "\n\tget_dim_names(nc) -> "
           << "does not match array, allocated " << dimCount << " but assigned " << i << ".\n\n";
   }
   return(status);
}

////////////////////////////////////////////////////////////////////////

bool get_dim_names(const NcVar *var, StringArray *dimNames) {

   int i, dimCount;
   NcDim dim;
   bool status = false;

   dimCount = var->getDimCount();

   i = 0;
   vector<NcDim>::iterator itDim;
   vector<NcDim> dims = var->getDims();
   for (itDim = dims.begin(); itDim != dims.end(); ++itDim) {
      dim = (*itDim);
      dimNames->add(dim.getName());
      i++;
   }

   if (i != dimCount) {
      mlog << Error << "\n\tget_dim_names(var) -> "
           << "does not match array, allocated " << dimCount << " but assigned " << i << ".\n\n";
   }
   return(status);
}

////////////////////////////////////////////////////////////////////////

vector<NcDim> get_dims(const NcVar *var, int *dim_count) {

   int dimCount;

   dimCount = var->getDimCount();
   *dim_count = dimCount;

   return var->getDims();
}

////////////////////////////////////////////////////////////////////////

bool is_nc_name_lat(const ConcatString name) {
   bool is_latitude = (name == "lat" || name == "LAT"
           || name == "Lat" || name == "Latitude"
           || name == "latitude" || name == "LATITUDE");
   return is_latitude;
}

////////////////////////////////////////////////////////////////////////

bool is_nc_name_lon(const ConcatString name) {
   bool is_longitude = (name == "lon" || name == "LON"
           || name == "Lon" || name == "Longitude"
           || name == "longitude" || name == "LONGITUDE");
   return is_longitude;
}

////////////////////////////////////////////////////////////////////////

bool is_nc_name_time(const ConcatString name) {
   bool is_time = (name == "t" || name == "time" || name == "Time" || name == "TIME"
           || name == "datetime" || name == "Datetime" || name == "DATETIME");
   return is_time;
}

////////////////////////////////////////////////////////////////////////

bool is_nc_attr_lat(const ConcatString name) {
   bool is_latitude = (is_nc_name_lat(name) || name == "x" || name == "X");
   return is_latitude;
}

////////////////////////////////////////////////////////////////////////

bool is_nc_attr_lon(const ConcatString name) {
   bool is_longitude = (is_nc_name_lon(name) || name == "y" || name == "Y");
   return is_longitude;
}

////////////////////////////////////////////////////////////////////////

bool is_nc_attr_time(const ConcatString name) {
   bool is_time = (is_nc_name_time(name) || name == "T");
   return is_time;
}

////////////////////////////////////////////////////////////////////////

NcVar get_nc_var_lat(const NcFile *nc) {
   NcVar var;
   bool found = false;
   multimap<string,NcVar> mapVar = GET_NC_VARS_P(nc);
   static const char *method_name = "get_nc_var_lat() ";

   for (multimap<string,NcVar>::iterator itVar = mapVar.begin();
        itVar != mapVar.end(); ++itVar) {
      ConcatString name = (*itVar).first;
      //if (is_nc_name_lat(name)) found = true;
      if (get_nc_att_value(&(*itVar).second, "standard_name", name)) {
         if (is_nc_name_lat(name)) found = true;
      }
      if (!found && get_nc_att_value(&(*itVar).second, "units", name)) {
         if (is_nc_unit_latitude(name.c_str())) {
            if (get_nc_att_value(&(*itVar).second, "axis", name)) {
               if (is_nc_attr_lat(name)) found = true;
            }
            else if (get_nc_att_value(&(*itVar).second, "_CoordinateAxisType", name)) {
               if (is_nc_attr_lat(name)) found = true;
            }
         }
      }
      if (found) {
         var = (*itVar).second;
         break;
      }
   }

   if (found) {
      mlog << Debug(6) << method_name << "found the latitude variable \""
           << GET_NC_NAME(var) << "\"\n";
   }
   else {
      mlog << Debug(6) << method_name << "fail to find the latitude variable\n";
   }
   return var;
}

////////////////////////////////////////////////////////////////////////

NcVar get_nc_var_lon(const NcFile *nc) {
   NcVar var;
   bool found = false;
   multimap<string,NcVar> mapVar = GET_NC_VARS_P(nc);
   static const char *method_name = "get_nc_var_lon() ";

   for (multimap<string,NcVar>::iterator itVar = mapVar.begin();
        itVar != mapVar.end(); ++itVar) {
      ConcatString name = (*itVar).first;
      //if (is_nc_name_lon(name)) found = true;
      if (get_nc_att_value(&(*itVar).second, "standard_name", name)) {
         if (is_nc_name_lon(name)) found = true;
      }
      if (!found && get_nc_att_value(&(*itVar).second, "units", name)) {
         if (is_nc_unit_longitude(name.c_str())) {
            if (get_nc_att_value(&(*itVar).second, "axis", name)) {
               if (is_nc_attr_lon(name)) found = true;
            }
            else if (get_nc_att_value(&(*itVar).second, "_CoordinateAxisType", name)) {
               if (is_nc_attr_lon(name)) found = true;
            }
         }
      }
      if (found) {
         var = (*itVar).second;
         break;
      }
   }

   if (found) {
      mlog << Debug(6) << method_name << "found the longitude variable \""
           << GET_NC_NAME(var) << "\"\n";
   }
   else {
      mlog << Debug(6) << method_name << "fail to find the longitude variable\n";
   }
   return var;
}

////////////////////////////////////////////////////////////////////////

NcVar get_nc_var_time(const NcFile *nc) {
   NcVar var;
   bool found = false;
   multimap<string,NcVar> mapVar = GET_NC_VARS_P(nc);
   static const char *method_name = "get_nc_var_time() ";

   for (multimap<string,NcVar>::iterator itVar = mapVar.begin();
        itVar != mapVar.end(); ++itVar) {
      ConcatString name = (*itVar).first;
      //if (is_nc_name_time(name)) found = true;
      if (get_nc_att_value(&(*itVar).second, "standard_name", name)) {
         if (is_nc_name_time(name)) found = true;
         mlog << Debug(7) << method_name << "checked variable \""
           << name << "\"  is_time: " << found << "\n";
      }
      if (!found && get_nc_att_value(&(*itVar).second, "units", name)) {
         if (is_nc_unit_time(name.c_str())) {
            if (get_nc_att_value(&(*itVar).second, "axis", name)) {
               if (is_nc_attr_time(name)) found = true;
            }
            else if (get_nc_att_value(&(*itVar).second, "_CoordinateAxisType", name)) {
               if (is_nc_attr_time(name)) found = true;
            }
         }
      }
      if (found) {
         var = (*itVar).second;
         break;
      }
   }

   if (found) {
      mlog << Debug(6) << method_name << "found the time variable \""
           << GET_NC_NAME(var) << "\"\n";
   }
   else {
      mlog << Debug(6) << method_name << "fail to find the time variable\n";
   }
   return var;
}


////////////////////////////////////////////////////////////////////////

NcFile *open_ncfile(const char * nc_name, bool write) {
   NcFile *nc = (NcFile *)0;

   try {
      if (write) {
         nc = new NcFile(nc_name, NcFile::replace, NcFile::nc4);
      }
      else {
         struct stat fileInfo;
         if (stat(nc_name, &fileInfo) == 0) {
            nc = new NcFile(nc_name, NcFile::read);
         }
      }
   }
   catch(NcException& e) {
   }
   return nc;
}

////////////////////////////////////////////////////////////////////////
// Implement the old API var->num_vals()

int get_data_size(NcVar *var) {
   int dimCount = 0;
   int data_size = 1;

   dimCount = var->getDimCount();
   for (int i=0; i<dimCount; i++) {
      data_size *= var->getDim(i).getSize();
   }
   return data_size;
}

////////////////////////////////////////////////////////////////////////

unixtime get_reference_unixtime(NcVar *time_var, int &sec_per_unit,
                                bool &no_leap_year) {
   unixtime ref_ut = 0;
   ConcatString time_unit_str;
   static const char *method_name = "get_reference_unixtime() -> ";

   if (get_nc_att_value(time_var, (string)"units", time_unit_str)) {
      parse_cf_time_string(time_unit_str.c_str(), ref_ut, sec_per_unit);
      no_leap_year = (86400 == sec_per_unit) ? get_att_no_leap_year(time_var) : false;
   }
   else {
      sec_per_unit = 1;
   }

   return ref_ut;
}

////////////////////////////////////////////////////////////////////////

bool is_nc_unit_latitude(const char *units) {
   bool axis_unit = (strcmp(units, "degrees_north") == 0 ||
        strcmp(units, "degree_north") == 0 ||
        strcmp(units, "degree_N") == 0 ||
        strcmp(units, "degrees_N") == 0 ||
        strcmp(units, "degreeN") == 0 ||
        strcmp(units, "degreesN") == 0);
   return axis_unit;
}

////////////////////////////////////////////////////////////////////////

bool is_nc_unit_longitude(const char *units) {
   bool axis_unit = (strcmp(units, "degrees_east") == 0 ||
        strcmp(units, "degree_east") == 0 ||
        strcmp(units, "degree_E") == 0 ||
        strcmp(units, "degrees_E") == 0 ||
        strcmp(units, "degreeE") == 0 ||
        strcmp(units, "degreesE") == 0);
   return axis_unit;
}

////////////////////////////////////////////////////////////////////////

bool is_nc_unit_time(const char *units) {
   return check_reg_exp(nc_time_unit_exp, units);
}

////////////////////////////////////////////////////////////////////////

void parse_cf_time_string(const char *str, unixtime &ref_ut,
                          int &sec_per_unit) {
   static const char *method_name = "parse_cf_time_string() -> ";

   // Initialize
   ref_ut = sec_per_unit = 0;

   // Check for expected time string format:
   //   [seconds|minutes|hours|days] since YYYY-MM-DD HH:MM:SS
   if(!check_reg_exp(nc_time_unit_exp, str)) {
      mlog << Warning << "\n" << method_name
           << "unexpected NetCDF CF convention time unit \""
           << str << "\"\n\n";
      return;
   }
   else {
      // Tokenize the input string
      // Parse using spaces or 'T' for timestrings such as:
      //   minutes since 2016-01-28T12:00:00Z
      //   seconds since 1977-08-07 12:00:00Z
      StringArray tok;
      tok.parse_delim(str, " T");
      tok.set_ignore_case(true);

      // Determine the time step
           if(tok.has("second")  ||
              tok.has("seconds") ||
              tok.has("s"))      sec_per_unit = 1;
      else if(tok.has("minute")  ||
              tok.has("minutes") ||
              tok.has("min"))    sec_per_unit = 60;
      else if(tok.has("hour")    ||
              tok.has("hours")   ||
              tok.has("hr")      ||
              tok.has("h"))      sec_per_unit = 3600;
      else if(tok.has("day")     ||
              tok.has("days")    ||
              tok.has("d"))      sec_per_unit = 86400;
      else {
         mlog << Warning << "\n" << method_name
              << "Unsupported time step in the CF convention time unit \""
              << str << "\"\n\n";
         return;
      }

      // Parse the reference time
      StringArray ymd, hms;
      ymd.parse_delim(tok[2], "-");
      if(tok.n_elements() > 3) hms.parse_delim(tok[3], ":");
      else                     hms.parse_delim("00:00:00", ":");
      ref_ut = mdyhms_to_unix(atoi(ymd[1].c_str()), atoi(ymd[2].c_str()),
                              atoi(ymd[0].c_str()), atoi(hms[0].c_str()),
                              hms.n_elements() > 1 ? atoi(hms[1].c_str()) : 0,
                              hms.n_elements() > 2 ? atoi(hms[2].c_str()) : 0);
   }

   mlog << Debug(4) << method_name
        << "parsed NetCDF CF convention time unit string \"" << str
        << "\"\n\t\t as a reference time of " << unix_to_yyyymmdd_hhmmss(ref_ut)
        << " and " << sec_per_unit << " second(s) per time step.\n";

   return;
}

////////////////////////////////////////////////////////////////////////
