// *=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*
// ** Copyright UCAR (c) 1992 - 2022
// ** University Corporation for Atmospheric Research (UCAR)
// ** National Center for Atmospheric Research (NCAR)
// ** Research Applications Lab (RAL)
// ** P.O.Box 3000, Boulder, Colorado, 80307-3000, USA
// *=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*

////////////////////////////////////////////////////////////////////////
//
//   Filename:   ioda2nc.cc
//
//   Description:
//      Based on user specified options, this tool filters point
//      observations from a IODA input file, derives requested
//      observation types, and writes the output to a NetCDF file.
//      IODA observations must be reformatted in this way prior to
//      using them in the Point-Stat tool.
//
//   Mod#   Date      Name           Description
//   ----   ----      ----           -----------
//   000    07-21-20  Howard Soh     New
//
////////////////////////////////////////////////////////////////////////

using namespace std;

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <ctype.h>
#include <iostream>
#include <fstream>
#include <limits>
#include <string.h>
#include <assert.h>

#include "apply_mask.h"
#include "ioda2nc_conf_info.h"
#include "vx_log.h"
#include "vx_nc_util.h"
#include "vx_util.h"
#include "vx_cal.h"
#include "vx_math.h"
#include "write_netcdf.h"

#include "vx_summary.h"
#include "nc_obs_util.h"
#include "nc_point_obs_out.h"
#include "nc_summary.h"

////////////////////////////////////////////////////////////////////////

//
// Constants
//

static const char * DEF_CONFIG_NAME = "MET_BASE/config/IODA2NCConfig_default";

static const char *program_name = "ioda2nc";

static const int REJECT_DEBUG_LEVEL = 9;

////////////////////////////////////////////////////////////////////////

//
// Variables for command line arguments
//

// StringArray to store IODA file name
static StringArray ioda_files;

static StringArray core_dims;
static StringArray core_vars;

// Output NetCDF file name
static ConcatString ncfile;

// Input configuration file
static ConcatString  config_file;
static IODA2NCConfInfo conf_info;

static Grid        mask_grid;
static MaskPlane   mask_area;
static MaskPoly    mask_poly;
static StringArray mask_sid;

// Beginning and ending retention times
static unixtime valid_beg_ut, valid_end_ut;

// Number of IODA messages to process from the command line
static int nmsg = -1;
static int nmsg_percent = -1;

static bool do_all_vars      = false;
static StringArray obs_var_names;
static StringArray obs_var_units;
static StringArray obs_var_descs;

static int compress_level = -1;

////////////////////////////////////////////////////////////////////////

static IntArray filtered_times;
//static map<ConcatString, StringArray> variableTypeMap;

static bool do_summary;
static bool save_summary_only = false;
static SummaryObs *summary_obs;
static MetNcPointObsOut nc_point_obs;


////////////////////////////////////////////////////////////////////////

static int n_total_obs;    // Running total of observations
static vector<Observation> observations;

//
// Output NetCDF file, dimensions, and variables
//
static NcFile *f_out = (NcFile *) 0;

////////////////////////////////////////////////////////////////////////

static void initialize();
static void process_command_line(int, char **);
static void open_netcdf();
static void process_ioda_file(int);
static void write_netcdf_hdr_data();
static void clean_up();

static void addObservation(const float *obs_arr, const ConcatString &hdr_typ,
      const ConcatString &hdr_sid, const time_t hdr_vld,
      const float hdr_lat, const float hdr_lon, const float hdr_elv,
      const float quality_mark, const int buf_size);

static bool keep_message_type(const char *);
static bool keep_station_id(const char *);
static bool keep_valid_time(const unixtime, const unixtime, const unixtime);

static void usage();
static void set_compress(const StringArray &);
static void set_config(const StringArray &);
static void set_ioda_files(const StringArray &);
static void set_mask_grid(const StringArray &);
static void set_mask_poly(const StringArray &);
static void set_mask_sid(const StringArray &);
static void set_logfile(const StringArray &);
static void set_nmsg(const StringArray &);
static void set_obs_var(const StringArray & a);
static void set_valid_beg_time(const StringArray &);
static void set_valid_end_time(const StringArray &);
static void set_verbosity(const StringArray &);

static bool check_core_data(const bool, const bool, StringArray &, StringArray &);
static bool check_missing_thresh(float value);
static ConcatString find_meta_name(StringArray, StringArray);
static bool get_meta_data_float(NcFile *, StringArray &, const char *, float *, const int);
static bool get_meta_data_strings(NcFile *, const ConcatString, char *);
static bool get_obs_data_float(NcFile *, const ConcatString, 
                               NcVar *, float *, int *, const int);
static bool has_postfix(std::string const &, std::string const &);
    
////////////////////////////////////////////////////////////////////////


int main(int argc, char *argv[]) {
   int i;

   // Set handler to be called for memory allocation error
   set_new_handler(oom);

   // Initialize static variables
   initialize();

   // Process the command line arguments
   process_command_line(argc, argv);

   // Open the NetCDF file
   open_netcdf();

   // Process each IODA file
   for(i=0; i<ioda_files.n_elements(); i++) {
      //process_ioda_file_metadata(i);
      process_ioda_file(i);
   }

   if(do_summary) {
      TimeSummaryInfo summaryInfo = conf_info.getSummaryInfo();
      summary_obs->summarizeObs(summaryInfo);
      summary_obs->setSummaryInfo(summaryInfo);
   }

   // Write the NetCDF file
   write_netcdf_hdr_data();

   // Deallocate memory and clean up
   clean_up();

   return(0);
}

////////////////////////////////////////////////////////////////////////

void initialize() {

   n_total_obs = 0;

   nc_point_obs.init_buffer();

   core_dims.clear();
   core_dims.add("nvars");
   core_dims.add("nlocs");
   //core_dims.add("nstring");
   //core_dims.add("ndatetime");

   core_vars.clear();
   core_vars.add("datetime");
   core_vars.add("latitude");
   core_vars.add("longitude");

   summary_obs = new SummaryObs();
   return;
}

////////////////////////////////////////////////////////////////////////

void process_command_line(int argc, char **argv) {
   CommandLine cline;
   static const char *method_name = "process_command_line() -> ";
   
   // Check for zero arguments
   if(argc == 1) usage();

   // Initialize retention times
   valid_beg_ut = valid_end_ut = (unixtime) 0;

   // Parse the command line into tokens
   cline.set(argc, argv);

   // Set the usage function
   cline.set_usage(usage);

   // Add the options function calls
   cline.add(set_ioda_files, "-iodafile", 1);
   cline.add(set_valid_beg_time, "-valid_beg", 1);
   cline.add(set_valid_end_time, "-valid_end", 1);
   cline.add(set_nmsg, "-nmsg", 1);
   cline.add(set_obs_var, "-obs_var", 1);
   cline.add(set_config,    "-config",    1);
   cline.add(set_mask_grid, "-mask_grid", 1);
   cline.add(set_mask_poly, "-mask_poly", 1);
   cline.add(set_mask_sid,  "-mask_sid",  1);
   cline.add(set_logfile, "-log", 1);
   cline.add(set_verbosity, "-v", 1);
   cline.add(set_compress,  "-compress",  1);

   // Parse the command line
   cline.parse();

   // Check for error. There should be three arguments left:
   // IODA, output NetCDF, and config filenames
   if(cline.n() < 2) usage();

   // Store the input file names
   ioda_files.add(cline[0]);
   if(cline.n() > 1) ncfile = cline[1];

   // Create the default config file name
   ConcatString default_config_file = replace_path(DEF_CONFIG_NAME);

   // List the config files and read the config files
   if(0 < config_file.length()) {
      if( !file_exists(config_file.c_str()) ) {
         mlog << Error << "\n" << method_name
              << "file does not exist \"" << config_file << "\"\n\n";
         exit(1);
      }
      mlog << Debug(1)
           << "Default Config File: " << default_config_file << "\n"
           << "User Config File: "    << config_file << "\n";
   }
   else mlog << Debug(1)
             << "Default Config File: " << default_config_file << "\n";
   conf_info.read_config(default_config_file.c_str(), config_file.c_str());

   // Process the configuration
   conf_info.process_config();

   // Check that valid_end_ut >= valid_beg_ut
   if(valid_beg_ut != (unixtime) 0 && valid_end_ut != (unixtime) 0
      && valid_beg_ut > valid_end_ut) {
      mlog << Error << "\nprocess_command_line() -> "
           << "the ending time (" << unix_to_yyyymmdd_hhmmss(valid_end_ut)
           << ") must be greater than the beginning time ("
           << unix_to_yyyymmdd_hhmmss(valid_beg_ut) << ").\n\n";
      exit(1);
   }

   do_summary = conf_info.getSummaryInfo().flag;
   if(do_summary) save_summary_only = !conf_info.getSummaryInfo().raw_data;
   else save_summary_only = false;

   return;
}

////////////////////////////////////////////////////////////////////////

void open_netcdf() {

   // Create the output netCDF file for writing
   mlog << Debug(1) << "Creating NetCDF File:\t\t" << ncfile << "\n";
   f_out = open_ncfile(ncfile.c_str(), true);

   // Check for a valid file
   if(IS_INVALID_NC_P(f_out)) {
      mlog << Error << "\nopen_netcdf() -> "
           << "trouble opening output file: " << ncfile << "\n\n";

      delete f_out;
      f_out = (NcFile *) 0;

      exit(1);
   }

   // Define netCDF variables
   int deflate_level = compress_level;
   if(deflate_level < 0) deflate_level = conf_info.conf.nc_compression();
   nc_point_obs.set_netcdf(f_out, true);
   nc_point_obs.init_obs_vars(true, deflate_level);

   // Add global attributes
   write_netcdf_global(f_out, ncfile.text(), program_name);

   return;
}

////////////////////////////////////////////////////////////////////////

void process_ioda_file(int i_pb) {
   int npbmsg, npbmsg_total;
   int idx, i_msg, i_read, n_file_obs, n_hdr_obs;
   int rej_typ, rej_sid, rej_vld, rej_grid, rej_poly;
   int rej_elv, rej_nobs;
   double   x, y;

   unixtime file_ut;
   unixtime adjusted_file_ut;
   unixtime msg_ut, beg_ut, end_ut;
   unixtime min_msg_ut, max_msg_ut;

   ConcatString file_name, blk_prefix, blk_file, log_message;
   ConcatString prefix;
   char     time_str[max_str_len];
   ConcatString  start_time_str, end_time_str;
   char     min_time_str[max_str_len], max_time_str[max_str_len];

   char     hdr_typ[max_str_len];
   ConcatString hdr_sid;
   char     modified_hdr_typ[max_str_len];
   double   hdr_lat, hdr_lon, hdr_elv;
   unixtime hdr_vld_ut;
   float    obs_arr[OBS_ARRAY_LEN];

   const int debug_level_for_performance = 3;
   int start_t, end_t, method_start, method_end;
   start_t = end_t = method_start = method_end = clock();

   IntArray diff_file_times;
   int diff_file_time_count;
   StringArray variables_big_nlevels;
   static const char *method_name = "process_ioda_file() -> ";
   static const char *method_name_s = "process_ioda_file() ";

   bool apply_grid_mask = (conf_info.grid_mask.nx() > 0 &&
                           conf_info.grid_mask.ny() > 0);
   bool apply_area_mask = (conf_info.area_mask.nx() > 0 &&
                           conf_info.area_mask.ny() > 0);
   bool apply_poly_mask = (conf_info.poly_mask.n_points() > 0);

   hdr_typ[0] = 0;

   file_ut = beg_ut = end_ut = hdr_vld_ut = (unixtime) 0;

   // List the IODA file being processed
   mlog << Debug(1) << "Processing IODA File:\t" << ioda_files[i_pb]<< "\n";

   NcFile *f_in = open_ncfile(ioda_files[i_pb].c_str());

   // Check for a valid file
   if(IS_INVALID_NC_P(f_in)) {
      mlog << Error << "\n" << method_name
           << "can't open input NetCDF file \"" << ioda_files[i_pb]
           << "\" for reading.\n\n";
      delete f_in;
      f_in = (NcFile *) 0;

      exit(1);
   }

   // Initialize
   filtered_times.clear();
   min_msg_ut = max_msg_ut = (unixtime) 0;
   min_time_str[0] = 0;
   max_time_str[0] = 0;
   modified_hdr_typ[0] = 0;

   // Set the file name for the IODA file
   file_name << ioda_files[i_pb];

   int nrecs = 0;
   //int nstring = 0;
   StringArray var_names, dim_names;
   StringArray metadata_vars;
   StringArray obs_value_vars;
   get_var_names(f_in, &var_names);
   get_dim_names(f_in, &dim_names);
   for(idx=0; idx<var_names.n(); idx++) {
      if(has_postfix(var_names[idx], "MetaData")) {
         metadata_vars.add(var_names[idx].substr(0, var_names[idx].find('@')));
      }
      if(has_postfix(var_names[idx], "ObsValue")) {
         obs_value_vars.add(var_names[idx].substr(0, var_names[idx].find('@')));
      }
   }
   if(mlog.verbosity_level() >= 6) {
      for(idx=0; idx<var_names.n(); idx++)
         mlog << Debug(6) << method_name << "var_name: " << var_names[idx] << "\n";
      for(idx=0; idx<dim_names.n(); idx++)
         mlog << Debug(6) << method_name << "dim_name: " << dim_names[idx] << "\n";
      for(idx=0; idx<metadata_vars.n(); idx++)
         mlog << Debug(6) << method_name << "metadata var: " << metadata_vars[idx] << "\n";
      for(idx=0; idx<obs_value_vars.n(); idx++)
         mlog << Debug(6) << method_name << "ObsValue var: " << obs_value_vars[idx] << "\n";
   }

   ConcatString msg_type_name = find_meta_name(metadata_vars,
                                               conf_info.metadata_map[conf_key_message_type]);
   ConcatString station_id_name = find_meta_name(metadata_vars,
                                                 conf_info.metadata_map[conf_key_station_id]);

   bool has_msg_type = 0 < msg_type_name.length();
   bool has_station_id = 0 < station_id_name.length();
   bool is_netcdf_ready = check_core_data(has_msg_type, has_station_id,
                                          dim_names, metadata_vars);

   if(!is_netcdf_ready) {
      mlog << Error << "\n" << method_name
           << "Please check the IODA file (required dimensions or meta variables are missing).\n\n";
      delete f_in;
      f_in = (NcFile *) 0;
      exit(1);
   }
   
   // Compute the number of IODA records in the current file.
   bool error_out = true;
   int nvars = get_dim_value(f_in, "nvars", error_out); // number of variables
   int nlocs = get_dim_value(f_in, "nlocs", error_out); // number of locations
   int nstring = get_dim_value(f_in, "nstring", error_out);
   
   if(dim_names.has("nrecs")) nrecs = get_dim_value(f_in, "nrecs", false);
   else {
      nrecs = nvars * nlocs;
      mlog << Debug(3) << "\n" << method_name
           << "nrecs dimension does not exist, so computed\n";
   }
   NcVar in_hdr_vld_var = get_var(f_in, "datetime@MetaData");
   NcVar in_hdr_lat_var = get_var(f_in, "latitude@MetaData");
   NcVar in_hdr_lon_var = get_var(f_in, "longitude@MetaData");
   
   int ndatetime;
   if(dim_names.has("ndatetime")) ndatetime = get_dim_value(f_in, "ndatetime", error_out);
   else {
      NcDim datetime_dim = get_nc_dim(&in_hdr_vld_var, 1);
      ndatetime = IS_VALID_NC(datetime_dim) ? get_dim_size(&datetime_dim) : nstring;
      mlog << Debug(3) << "\n" << method_name
           << "ndatetime dimension does not exist!\n";
   }
   mlog << Debug(5) << method_name << "dimensions: nvars=" << nvars << ", nlocs=" << nlocs
        << ", nrecs=" << nrecs << ", nstring=" << nstring << ", ndatetime=" << ndatetime << "\n";

   npbmsg_total = npbmsg = nlocs;

   // Use the number of records requested by the user if there
   // are enough present.
   if(nmsg > 0 && nmsg < npbmsg) {
      npbmsg = (nmsg_percent > 0 && nmsg_percent <= 100)
            ? (npbmsg * nmsg_percent / 100) : nmsg;
   }

   long lengths[2] = { nlocs, ndatetime };
   long offsets[2] = { 0, 0 };
   float *hdr_lat_arr = new float[nlocs];
   float *hdr_lon_arr = new float[nlocs];
   float *hdr_elv_arr = new float[nlocs];
   float *obs_pres_arr = new float[nlocs];
   float *obs_hght_arr = new float[nlocs];
   char *hdr_vld_block = new char[nlocs*ndatetime];
   char *hdr_msg_types = 0;
   char *hdr_station_ids = 0;
   vector<int *> v_qc_data;
   vector<float *> v_obs_data;
   
   get_meta_data_float(f_in, metadata_vars, "pressure", obs_pres_arr, nlocs);
   get_meta_data_float(f_in, metadata_vars, "height", obs_hght_arr, nlocs);
   get_meta_data_float(f_in, metadata_vars, "elevation", hdr_elv_arr, nlocs);

   if(has_msg_type) {
      hdr_msg_types = new char[nlocs*nstring];
      get_meta_data_strings(f_in, msg_type_name, hdr_msg_types);
   }

   if(has_station_id) {
      hdr_station_ids = new char[nlocs*nstring];
      get_meta_data_strings(f_in, station_id_name, hdr_station_ids);
   }

   if(!get_nc_data(&in_hdr_lat_var, hdr_lat_arr, nlocs)) {
      mlog << Error << "\n" << method_name
           << "trouble getting latitude\n\n";
      exit(1);
   }
   if(!get_nc_data(&in_hdr_lon_var, hdr_lon_arr, nlocs)) {
      mlog << Error << "\n" << method_name
           << "trouble getting longitude\n\n";
      exit(1);
   }
   if(!get_nc_data(&in_hdr_vld_var, hdr_vld_block, lengths, offsets)) {
      mlog << Error << "\n" << method_name
           << "trouble getting datetime\n\n";
      exit(1);
   }

   StringArray raw_var_names;
   if(do_all_vars || obs_var_names.n() == 0) raw_var_names = obs_value_vars;
   else raw_var_names = obs_var_names;

   NcVar obs_var, qc_var;
   ConcatString unit_attr;
   ConcatString desc_attr;
   for(idx=0; idx<raw_var_names.n(); idx++ ) {
      int *qc_data = new int[nlocs];
      float *obs_data = new float[nlocs];
      ConcatString obs_var_name = raw_var_names[idx] + "@ObsValue";

      mlog << Debug(7) << method_name
           << "processing \"" << obs_var_name << "\" variable!\n";
      obs_var = get_var(f_in, obs_var_name.c_str());
      v_qc_data.push_back(qc_data);
      v_obs_data.push_back(obs_data);
      unit_attr.clear();
      desc_attr.clear();
      get_obs_data_float(f_in, raw_var_names[idx], &obs_var, obs_data, qc_data, nlocs);
      if(IS_VALID_NC(obs_var)) {
         get_var_units(&obs_var, unit_attr);
         get_att_value_string(&obs_var, "long_name", desc_attr);
      }

      // Replace the input variable name to the output variable name
      ConcatString raw_name = raw_var_names[idx];
      // Filter out the same variable names from multiple input files
      if (!obs_var_names.has(raw_name)) {
         obs_var_names.add(raw_name);
         obs_var_units.add(unit_attr);
         obs_var_descs.add(desc_attr);
      }
   }

   // Initialize counts
   n_file_obs = i_msg = 0;
   rej_typ = rej_sid    = rej_vld    = rej_grid = rej_poly = 0;
   rej_elv = rej_nobs   = 0;

   bool showed_progress = false;
   if(mlog.verbosity_level() >= debug_level_for_performance) {
      end_t = clock();
      mlog << Debug(debug_level_for_performance) << " PERF: " << method_name_s << " "
           << (end_t-start_t)/double(CLOCKS_PER_SEC)
           << " seconds for preparing\n";
      start_t = clock();
   }

   log_message.add(" IODA messages");
   if(npbmsg != npbmsg_total) {
      log_message << " (out of " << unixtime_to_string(npbmsg_total) << ")";
   }
   mlog << Debug(2) << "Processing " << npbmsg << log_message << "...\n";

   int      bin_count = nint(npbmsg/20.0);
   unixtime prev_hdr_vld_ut = (unixtime) 0;
   map<ConcatString, ConcatString> message_type_map = conf_info.getMessageTypeMap();

   // Initialize
   diff_file_time_count = 0;

   for(int idx=0; idx<OBS_ARRAY_LEN; idx++) obs_arr[idx] = 0;

   // Loop through the IODA messages from the input file
   for(i_read=0; i_read<npbmsg; i_read++) {

      if(mlog.verbosity_level() > 0) {
         if(bin_count > 0 && (i_read+1)%bin_count == 0) {
            cout << nint((double) (i_read+1)/npbmsg*100.0) << "% " << flush;
            showed_progress = true;
            if(mlog.verbosity_level() >= debug_level_for_performance) {
               end_t = clock();
               cout << (end_t-start_t)/double(CLOCKS_PER_SEC)
                    << " seconds\n";
               start_t = clock();
            }
         }
      }

      char valid_time[ndatetime+1];
      m_strncpy(valid_time, (const char *)(hdr_vld_block + (i_read * ndatetime)),
                ndatetime, method_name_s, "valid_time", true);
      valid_time[ndatetime] = 0;
      msg_ut = yyyymmddThhmmss_to_unix(valid_time);

      // Check to make sure that the message time hasn't changed
      // from one IODA message to the next
      if(file_ut == (unixtime) 0) {
         adjusted_file_ut = file_ut = msg_ut;

         mlog << Debug(2)
              << " IODA Time Center:\t\t" << unix_to_yyyymmdd_hhmmss(adjusted_file_ut)
              << "\n";

         // Check if valid_beg_ut and valid_end_ut were set on the
         // command line.  If so, use them.  If not, use beg_ds and
         // end_ds.
         if(valid_beg_ut != (unixtime) 0 ||
            valid_end_ut != (unixtime) 0) {
            beg_ut = valid_beg_ut;
            end_ut = valid_end_ut;
         }
         else {
            beg_ut = adjusted_file_ut + conf_info.beg_ds;
            end_ut = adjusted_file_ut + conf_info.end_ds;
         }

         if(beg_ut != (unixtime) 0) {
            unix_to_yyyymmdd_hhmmss(beg_ut, start_time_str);
         }
         else {
            start_time_str = "NO_BEG_TIME";
         }

         if(end_ut != (unixtime) 0) {
            unix_to_yyyymmdd_hhmmss(end_ut, end_time_str);
         }
         else {
            end_time_str = "NO_END_TIME";
         }

         mlog << Debug(2) << "Searching Time Window:\t\t" << start_time_str
              << " to " << end_time_str << "\n";
      }
      else if(file_ut != msg_ut) {
         diff_file_time_count++;
         if(!diff_file_times.has(msg_ut)) diff_file_times.add(msg_ut);
      }

      if(has_msg_type) {
         int buf_len = sizeof(modified_hdr_typ);
         m_strncpy(hdr_typ, hdr_msg_types+(i_read*nstring), nstring, method_name_s, "hdr_typ");
         m_rstrip(hdr_typ, nstring);

         // If the message type is not listed in the configuration
         // file and it is not the case that all message types should be
         // retained, continue to the next IODA message
         if(!keep_message_type(hdr_typ)) {
            rej_typ++;
            continue;
         }

         if(0 < message_type_map.count((string)hdr_typ)) {
            ConcatString mappedMessageType = message_type_map[(string)hdr_typ];
            mlog << Debug(6) << "\n" << method_name
                 << "Switching report type \"" << hdr_typ
                 << "\" to message type \"" << mappedMessageType << "\".\n";
            if(mappedMessageType.length() < HEADER_STR_LEN) buf_len = HEADER_STR_LEN;
            m_strncpy(modified_hdr_typ, mappedMessageType.c_str(), buf_len,
                      method_name_s, "modified_hdr_typ");
         }
         else {
            m_strncpy(modified_hdr_typ, hdr_typ, buf_len, method_name_s, "modified_hdr_typ2");
         }
         modified_hdr_typ[buf_len-1] = 0;
      }

      if(has_station_id) {
         char tmp_sid[nstring+1];
         m_strncpy(tmp_sid, hdr_station_ids+(i_read*nstring), nstring, method_name_s, "tmp_sid");
         m_rstrip(tmp_sid, nstring);
         hdr_sid = tmp_sid;
      }
      else hdr_sid.clear();

      // If the station id is not listed in the configuration
      // file and it is not the case that all station ids should be
      // retained, continue to the next IODA message
      if(!keep_station_id(hdr_sid.c_str())) {
         rej_sid++;
         continue;
      }

      // Read the header array elements which consists of:
      //    LON LAT DHR ELV TYP T29 ITP

      // Longitude
      hdr_lon = hdr_lon_arr[i_read];

      // Latitude
      hdr_lat = hdr_lat_arr[i_read];

      // Elevation
      hdr_elv = hdr_elv_arr[i_read];

      // Compute the valid time and check if it is within the
      // specified valid range
      //hdr_vld_ut = msg_ut + (unixtime)nint(hdr[3]*sec_per_hour);
      hdr_vld_ut = msg_ut;
      if(0 == min_msg_ut || min_msg_ut > hdr_vld_ut) min_msg_ut = hdr_vld_ut;
      if(max_msg_ut < hdr_vld_ut) max_msg_ut = hdr_vld_ut;
      if(!keep_valid_time(hdr_vld_ut, beg_ut, end_ut)) {
         if(!filtered_times.has(hdr_vld_ut, false)) {
            filtered_times.add(hdr_vld_ut);
         }
         rej_vld++;
         continue;
      }

      // Rescale the longitude value from 0 to 360 -> -180 to 180
      hdr_lon = rescale_lon(hdr_lon);

      // If the lat/lon for the IODA message is not on the
      // grid_mask, continue to the next IODA message
      if(apply_grid_mask) {
         conf_info.grid_mask.latlon_to_xy(hdr_lat, (-1.0*hdr_lon), x, y);
         if(x < 0 || x >= conf_info.grid_mask.nx() ||
            y < 0 || y >= conf_info.grid_mask.ny()) {
            rej_grid++;
            continue;
         }

         // Include the area mask rejection counts with the polyline since
         // it is specified using the mask.poly config option.
         if(apply_area_mask) {
            if(!conf_info.area_mask.s_is_on(nint(x), nint(y))) {
               rej_poly++;
               continue;
            }
         }
      }

      // If the lat/lon for the IODA message is not inside the mask
      // polyline continue to the next IODA message.  Multiply by
      // -1 to convert from degrees_east to degrees_west
      if(apply_poly_mask &&
            !conf_info.poly_mask.latlon_is_inside_dege(hdr_lat, hdr_lon)) {
         rej_poly++;
         continue;
      }

      // Check if the message elevation is within the specified range.
      // Missing data values for elevation are retained.
      if(!check_missing_thresh(hdr_elv) &&
         (hdr_elv < conf_info.beg_elev || hdr_elv > conf_info.end_elev) ) {
         rej_elv++;
         continue;
      }

      // Store the index to the header data
      obs_arr[0] = (float) nc_point_obs.get_hdr_index();

      n_hdr_obs = 0;
      for(idx=0; idx<v_obs_data.size(); idx++ ) {
         int var_idx = 0;
         if (!obs_var_names.has(raw_var_names[idx], var_idx)) {
            mlog << Warning << "\n" << method_name
                 << "Skip the variable " << raw_var_names[idx]
                 << " at " << ioda_files[i_pb] << "\n\n";
            continue;
         }
         obs_arr[1] = var_idx;
         obs_arr[2] = obs_pres_arr[i_read];
         obs_arr[3] = obs_hght_arr[i_read];
         obs_arr[4] = v_obs_data[idx][i_read];
         addObservation(obs_arr, (string)hdr_typ, (string)hdr_sid, hdr_vld_ut,
               hdr_lat, hdr_lon, hdr_elv, (float)v_qc_data[idx][i_read],
               OBS_BUFFER_SIZE);

         // Increment the current and total observations counts
         n_file_obs++;
         n_total_obs++;

         // Increment the number of obs counter for this header
         n_hdr_obs++;
      } // end for idx

      // If the number of observations for this header is non-zero,
      // store the header data and increment the IODA record
      // counter
      if(n_hdr_obs > 0) {
         i_msg++;
      }
      else {
         rej_nobs++;
      }
   } // end for i_read

   if(showed_progress) {
      log_message = "100% ";
      if(mlog.verbosity_level() >= debug_level_for_performance) {
         end_t = clock();
         log_message << (end_t-start_t)/double(CLOCKS_PER_SEC) << " seconds";
      }
      cout << log_message << "\n";
   }

   nc_point_obs.write_observation();

   if(mlog.verbosity_level() > 0) cout << "\n" << flush;

   mlog << Debug(2)
        << "Total records processed\t\t= " << npbmsg << "\n"
        << "Rejected based on message type\t\t= "
        << rej_typ << "\n"
        << "Rejected based on station id\t\t= "
        << rej_sid << "\n"
        << "Rejected based on valid time\t\t= "
        << rej_vld << "\n"
        << "Rejected based on masking grid\t\t= "
        << rej_grid << "\n"
        << "Rejected based on masking polygon\t= "
        << rej_poly << "\n"
        << "Rejected based on elevation\t\t= "
        << rej_elv << "\n"
        << "Rejected based on zero observations\t= "
        << rej_nobs << "\n"
        << "Total Records retained\t\t\t= "
        << i_msg << "\n"
        << "Total observations retained or derived\t= "
        << n_file_obs << "\n";

   if(npbmsg == rej_vld && 0 < rej_vld) {
      mlog << Warning << "\n" << method_name
           << "All records were filtered out by valid time.\n"
           << "\tPlease adjust time range with \"-valid_beg\" and \"-valid_end\".\n"
           << "\tmin/max obs time from IODA file: " << min_time_str
           << " and " << max_time_str << ".\n"
           << "\ttime range: " << start_time_str << " and " << end_time_str << ".\n\n";
   }
   else {
      mlog << Debug(1) << "Obs time between " << unix_to_yyyymmdd_hhmmss(min_msg_ut)
           << " and " << unix_to_yyyymmdd_hhmmss(max_msg_ut) << "\n";

      int debug_level = 5;
      if(mlog.verbosity_level() >= debug_level) {
         log_message = "Filtered time:";
         for(int kk=0; kk<filtered_times.n_elements();kk++) {
            log_message.add((0 == (kk % 3)) ? "\n\t" : "  ");
            log_message.add(unix_to_yyyymmdd_hhmmss(filtered_times[kk]));
         }
         mlog << Debug(debug_level) << log_message << "\n";
      }
   }
   
   delete [] hdr_lat_arr;
   delete [] hdr_lon_arr;
   delete [] hdr_elv_arr;
   delete [] obs_pres_arr;
   delete [] obs_hght_arr;
   if (hdr_msg_types) delete [] hdr_msg_types;
   if (hdr_station_ids) delete [] hdr_station_ids;
   delete [] hdr_vld_block;
   for(idx=0; idx<v_obs_data.size(); idx++ ) delete [] v_obs_data[idx];
   for(idx=0; idx<v_qc_data.size(); idx++ ) delete [] v_qc_data[idx];
   v_obs_data.clear();
   v_qc_data.clear();

   // Close the input NetCDF file
   if(f_in) {
      delete f_in;
      f_in = (NcFile *) 0;
   }

   if(mlog.verbosity_level() >= debug_level_for_performance) {
      method_end = clock();
      cout << " PERF: " << method_name_s
           << (method_end-method_start)/double(CLOCKS_PER_SEC)
           << " seconds\n";
   }

   if(i_msg <= 0) {
      mlog << Warning << "\n" << method_name
           << "No IODA records retained from file: "
           << ioda_files[i_pb] << "\n\n";
   }

   return;
}

////////////////////////////////////////////////////////////////////////

void write_netcdf_hdr_data() {
   int obs_cnt, hdr_cnt;
   const long hdr_count = (long) nc_point_obs.get_hdr_index();
   static const string method_name = "\nwrite_netcdf_hdr_data()";

   nc_point_obs.set_nc_out_data(observations, summary_obs, conf_info.getSummaryInfo());
   nc_point_obs.get_dim_counts(&obs_cnt, &hdr_cnt);
   nc_point_obs.init_netcdf(obs_cnt, hdr_cnt, program_name);

   // Check for no messages retained
   if(hdr_cnt <= 0) {
      mlog << Error << method_name << " -> "
           << "No IODA records retained.  Nothing to write.\n\n";
      // Delete the NetCDF file
      remove_temp_file(ncfile);
      exit(1);
   }

   // Make sure all obs data is processed before handling header
   StringArray nc_var_name_arr;
   StringArray nc_var_unit_arr;
   StringArray nc_var_desc_arr;
   const long var_count = obs_var_names.n();
   const long units_count = obs_var_units.n();
   map<ConcatString,ConcatString> name_map = conf_info.getObsVarMap();

   for(int i=0; i<var_count; i++) {
      ConcatString new_name = name_map[obs_var_names[i]];
      if (0 >= new_name.length()) new_name = obs_var_names[i];
      nc_var_name_arr.add(new_name);
   }
   for(int i=0; i<units_count; i++) {
      nc_var_unit_arr.add(obs_var_units[i]);
   }
   for(int i=0; i<obs_var_descs.n(); i++) {
      nc_var_desc_arr.add(obs_var_descs[i]);
   }

   nc_point_obs.write_to_netcdf(nc_var_name_arr, nc_var_unit_arr, nc_var_desc_arr);

   return;
}

////////////////////////////////////////////////////////////////////////

void addObservation(const float *obs_arr, const ConcatString &hdr_typ,
      const ConcatString &hdr_sid, const time_t hdr_vld,
      const float hdr_lat, const float hdr_lon, const float hdr_elv,
      const float quality_mark, const int buf_size)
{
   // Write the quality flag to the netCDF file
   ConcatString obs_qty;
   if(check_missing_thresh(quality_mark))
      obs_qty.add("NA");
   else obs_qty.format("%d", nint(quality_mark));

   int var_index = obs_arr[1];
   map<ConcatString,ConcatString> name_map = conf_info.getObsVarMap();
   string var_name = obs_var_names[var_index];
   string out_name = name_map[var_name];
   Observation obs = Observation(hdr_typ.text(),
                                 hdr_sid.text(),
                                 hdr_vld,
                                 hdr_lat, hdr_lon, hdr_elv,
                                 obs_qty.text(),
                                 var_index,
                                 obs_arr[2], obs_arr[3], obs_arr[4],
                                 (0<out_name.length() ? out_name : var_name));
   obs.setHeaderIndex(obs_arr[0]);
   observations.push_back(obs);
   if(do_summary) summary_obs->addObservationObj(obs);
   return;
}

////////////////////////////////////////////////////////////////////////

void clean_up() {

   nc_point_obs.close();

   if(f_out) {
      delete f_out;
      f_out = (NcFile *) 0;
   }

   return;
}

////////////////////////////////////////////////////////////////////////

bool keep_message_type(const char *mt_str) {
   bool keep = conf_info.message_type.n_elements() == 0 ||
               conf_info.message_type.has(mt_str, false);

   if(!keep && mlog.verbosity_level() >= REJECT_DEBUG_LEVEL) {
      mlog << Debug(REJECT_DEBUG_LEVEL) << "The message type [" << mt_str << "] is rejected\n";
   }

   return(keep);
}

////////////////////////////////////////////////////////////////////////

bool keep_station_id(const char *sid_str) {

   bool keep = (conf_info.station_id.n_elements() == 0 ||
                conf_info.station_id.has(sid_str, false));

   if(!keep && mlog.verbosity_level() >= REJECT_DEBUG_LEVEL) {
      mlog << Debug(REJECT_DEBUG_LEVEL) << "The station ID [" << sid_str << "] is rejected\n";
   }

   return(keep);
}

////////////////////////////////////////////////////////////////////////

bool keep_valid_time(const unixtime ut,
                     const unixtime min_ut, const unixtime max_ut) {
   bool keep = true;

   // If min_ut and max_ut both set, check the range
   if(min_ut != (unixtime) 0 && max_ut != (unixtime) 0) {
      if(ut < min_ut || ut > max_ut) keep = false;
   }
   // If only min_ut set, check the lower bound
   else if(min_ut != (unixtime) 0 && max_ut == (unixtime) 0) {
      if(ut < min_ut) keep = false;
   }
   // If only max_ut set, check the upper bound
   else if(min_ut == (unixtime) 0 && max_ut != (unixtime) 0) {
      if(ut > max_ut) keep = false;
   }

   if(!keep && mlog.verbosity_level() >= REJECT_DEBUG_LEVEL) {
      mlog << Debug(REJECT_DEBUG_LEVEL) << "The valid_time [" << ut << ", "
           << unix_to_yyyymmdd_hhmmss(ut) << "] is rejected\n";
   }

   return(keep);
}

////////////////////////////////////////////////////////////////////////

bool check_core_data(const bool has_msg_type, const bool has_station_id,
                     StringArray &dim_names, StringArray &metadata_vars) {
   bool is_netcdf_ready = true;
   static const char *method_name = "check_core_data() -> ";
   
   for(int idx=0; idx<core_dims.n(); idx++) {
      if(!dim_names.has(core_dims[idx])) {
         mlog << Error << "\n" << method_name << "-> "
              << "core dimension \"" << core_dims[idx] << "\" is missing.\n\n";
         is_netcdf_ready = false;
      }
   }

   if(has_msg_type || has_station_id) {
      if(!dim_names.has("nstring")) {
         mlog << Error << "\n" << method_name << "-> "
              << "core dimension \"nstring\" is missing.\n\n";
         is_netcdf_ready = false;
      }
   }

   for(int idx=0; idx<core_vars.n(); idx++) {
      if(!metadata_vars.has(core_vars[idx])) {
         mlog << Error << "\n" << method_name << "-> "
              << "core variable  \"" << core_vars[idx] << "\" is missing.\n\n";
         is_netcdf_ready = false;
      }
   }
   return is_netcdf_ready;
}

////////////////////////////////////////////////////////////////////////

bool check_missing_thresh(float value) {
   bool check = false;
   for(int idx=0; idx<conf_info.missing_thresh.n(); idx++) {
      if(conf_info.missing_thresh[idx].check(value)) {
          check = true;
          break;
      }
   }
   return check;
}

////////////////////////////////////////////////////////////////////////

ConcatString find_meta_name(StringArray metadata_names, StringArray config_names) {
   ConcatString metadata_name;
   
   for(int idx =0; idx<config_names.n(); idx++) {
      if(metadata_names.has(config_names[idx])) {
         metadata_name = config_names[idx];
         break;
      }
   }
   return metadata_name;
}

////////////////////////////////////////////////////////////////////////

bool get_meta_data_float(NcFile *f_in, StringArray &metadata_vars,
                         const char *metadata_key, float *metadata_buf,
                         const int nlocs) {
   bool status = false;
   static const char *method_name = "get_meta_data_float() -> ";
   
   ConcatString metadata_name = find_meta_name(
        metadata_vars, conf_info.metadata_map[metadata_key]);
   if(metadata_name.length() > 0) {
      ConcatString ioda_name = metadata_name;
      ioda_name.add("@MetaData");
      NcVar meta_var = get_var(f_in, ioda_name.c_str());
      if(IS_VALID_NC(meta_var)) {
         status = get_nc_data(&meta_var, metadata_buf, nlocs);
         if(!status) mlog << Debug(3) << method_name
                           << "trouble getting " << metadata_name << "\n";
      }
   }
   else mlog << Debug(4) << method_name
             << "Metadata for " << metadata_key << " does not exist!\n";
   if(status) {
      for(int idx=0; idx<nlocs; idx++)
         if(check_missing_thresh(metadata_buf[idx])) metadata_buf[idx] = bad_data_float;
   }
   else {
      for(int idx=0; idx<nlocs; idx++)
         metadata_buf[idx] = bad_data_float;
   }
   return status;
}

////////////////////////////////////////////////////////////////////////

bool get_meta_data_strings(NcFile *f_in, const ConcatString metadata_name,
                           char *metadata_buf) {
   bool status = false;
   static const char *method_name = "get_meta_data_strings() -> ";

   ConcatString ioda_name = metadata_name;
   ioda_name.add("@MetaData");
   NcVar meta_var = get_var(f_in, ioda_name.c_str());
   if(IS_VALID_NC(meta_var)) {
      status = get_nc_data(&meta_var, metadata_buf);
      if(!status) {
         mlog << Error << "\n" << method_name << " -> "
              << "trouble getting " << metadata_name << "\n\n";
         exit(1);
      }
   }
   return status;
}

////////////////////////////////////////////////////////////////////////

bool get_obs_data_float(NcFile *f_in, const ConcatString var_name,
                        NcVar *obs_var, float *obs_buf, int *qc_buf,
                        const int nlocs) {
   bool status = false;
   static const char *method_name = "get_obs_data_float() -> ";
   
   if(IS_VALID_NC_P(obs_var)) {
      status = get_nc_data(obs_var, obs_buf, nlocs);
      if(status) {
         for(int idx=0; idx<nlocs; idx++)
            if(check_missing_thresh(obs_buf[idx])) obs_buf[idx] = bad_data_float;
      }
      else mlog << Error << "\n" << method_name
                << "trouble getting " << var_name << "\n\n";
   }
   else mlog << Error << "\n" << method_name
             << var_name << "@ObsValue does not exist!\n\n";
   if(!status) exit(1);
   
   status = false;
   if(var_name.length() > 0) {
      ConcatString ioda_name = var_name;
      ioda_name.add("@PreQC");
      NcVar qc_var = get_var(f_in, ioda_name.c_str());
      if(IS_VALID_NC(qc_var)) {
         status = get_nc_data(&qc_var, qc_buf, nlocs);
         if(!status) mlog << Debug(4) << method_name
                          << "trouble getting " << ioda_name << "\n";
      }
      else mlog << Debug(4) << method_name
                << "\"" << ioda_name << "\" does not exist!\n";
   }
   if(status) {
      for(int idx=0; idx<nlocs; idx++)
         if(check_missing_thresh(qc_buf[idx])) qc_buf[idx] = bad_data_float;
   }
   else {
      for(int idx=0; idx<nlocs; idx++)
         qc_buf[idx] = bad_data_int;
   }
   return status;
}

////////////////////////////////////////////////////////////////////////

bool has_postfix(std::string const &str_buf, std::string const &postfix) {
   if(str_buf.length() >= postfix.length()) {
      return (0 == str_buf.compare(str_buf.length() - postfix.length(), postfix.length(), postfix));
   } else {
      return false;
   }
}

////////////////////////////////////////////////////////////////////////

void usage() {

   cout << "\n*** Model Evaluation Tools (MET" << met_version
        << ") ***\n\n"

        << "Usage: " << program_name << "\n"
        << "\tioda_file\n"
        << "\tnetcdf_file\n"
        << "\t[-config config_file]\n"
        << "\t[-obs_var var]\n"
        << "\t[-iodafile ioda_file]\n"
        << "\t[-valid_beg time]\n"
        << "\t[-valid_end time]\n"
        << "\t[-nmsg n]\n"
        << "\t[-log file]\n"
        << "\t[-v level]\n"
        << "\t[-compress level]\n\n"

        << "\twhere\t\"ioda_file\" is the input IODA "
        << "observation file to be converted to netCDF format "
        << "(required).\n"

        << "\t\t\"netcdf_file\" indicates the name of the output "
        << "netCDF file to be written (required).\n"

        << "\t\t\"-config config_file\" is a IODA2NCConfig file containing the "
        << "desired configuration settings (optional).\n"

        << "\t\t\"-obs_var var1,...\" or multiple \"-obs_var var\" sets the variable list to be saved"
        << " from input IODA observation files (optional, default: all).\n"

        << "\t\t\"-iodafile ioda_file\" may be used to specify "
        << "additional input IODA observation files to be used "
        << "(optional).\n"

        << "\t\t\"-valid_beg time\" in YYYYMMDD[_HH[MMSS]] sets the "
        << "beginning of the retention time window (optional).\n"

        << "\t\t\"-valid_end time\" in YYYYMMDD[_HH[MMSS]] sets the "
        << "end of the retention time window (optional).\n"

        << "\t\t\"-nmsg n\" indicates the number of IODA records "
        << "to process (optional).\n"

        << "\t\t\"-log file\" outputs log messages to the specified "
        << "file (optional).\n"

        << "\t\t\"-v level\" overrides the default level of logging ("
        << mlog.verbosity_level() << ") (optional).\n"

        << "\t\t\"-compress level\" overrides the compression level of NetCDF variable ("
        << conf_info.conf.nc_compression() << ") (optional).\n\n"

        << flush;

   exit(1);
}

////////////////////////////////////////////////////////////////////////

void set_ioda_files(const StringArray & a)
{
   ioda_files.add(a[0]);
}

////////////////////////////////////////////////////////////////////////

void set_valid_beg_time(const StringArray & a)
{
   valid_beg_ut = timestring_to_unix(a[0].c_str());
}

////////////////////////////////////////////////////////////////////////

void set_valid_end_time(const StringArray & a)
{
   valid_end_ut = timestring_to_unix(a[0].c_str());
}

////////////////////////////////////////////////////////////////////////

void set_nmsg(const StringArray & a)
{
   nmsg = atoi(a[0].c_str());
   int tmp_len = a[0].length();
   if(1 < tmp_len) {
      if(a[0][tmp_len-1] == '%') {
         nmsg_percent = nmsg;
      }
   }
}

////////////////////////////////////////////////////////////////////////

void set_config(const StringArray & a) {
   config_file = a[0];
}

////////////////////////////////////////////////////////////////////////

void set_mask_grid(const StringArray & a) {

   // List the grid masking file
   mlog << Debug(1)
        << "Grid Masking: " << a[0] << "\n";

   parse_grid_mask(a[0], mask_grid);

   // List the grid mask
   mlog << Debug(2)
        << "Parsed Masking Grid: " << mask_grid.name() << " ("
        << mask_grid.nx() << " x " << mask_grid.ny() << ")\n";
}

////////////////////////////////////////////////////////////////////////

void set_mask_poly(const StringArray & a) {
   ConcatString mask_name;

   // List the poly masking file
   mlog << Debug(1)
        << "Polyline Masking File: " << a[0] << "\n";

   parse_poly_mask(a[0], mask_poly, mask_grid, mask_area, mask_name);

   // List the mask information
   if(mask_poly.n_points() > 0) {
      mlog << Debug(2)
           << "Parsed Masking Polyline: " << mask_poly.name()
           << " containing " <<  mask_poly.n_points() << " points\n";
   }

   // List the area mask information
   if(mask_area.nx() > 0 || mask_area.ny() > 0) {
      mlog << Debug(2)
           << "Parsed Masking Area: " << mask_name
           << " for (" << mask_grid.nx() << " x " << mask_grid.ny()
           << ") grid\n";
   }

   return;
}

////////////////////////////////////////////////////////////////////////

void set_mask_sid(const StringArray & a) {
   ConcatString mask_name;

   // List the station ID mask
   mlog << Debug(1) << "Station ID Mask: " << a[0] << "\n";

   parse_sid_mask(a[0], mask_sid, mask_name);

   // List the length of the station ID mask
   mlog << Debug(2) << "Parsed Station ID Mask: " << mask_name
        << " containing " << mask_sid.n_elements() << " points\n";
}

////////////////////////////////////////////////////////////////////////

void set_obs_var(const StringArray & a)
{
   if("_all_" == a[0] || "all" == a[0]) do_all_vars = true;
   else {
      ConcatString arg = a[0];
      obs_var_names.add(arg.split(",+ "));
   }
}

////////////////////////////////////////////////////////////////////////

void set_logfile(const StringArray & a)
{
   ConcatString filename;

   filename = a[0];

   mlog.open_log_file(filename);
}

////////////////////////////////////////////////////////////////////////

void set_verbosity(const StringArray & a)
{
   mlog.set_verbosity_level(atoi(a[0].c_str()));
}

////////////////////////////////////////////////////////////////////////

void set_compress(const StringArray & a) {
   compress_level = atoi(a[0].c_str());
}

////////////////////////////////////////////////////////////////////////
