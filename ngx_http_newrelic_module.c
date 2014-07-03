#include "ngx_http_newrelic_module.h"

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>
#include <ngx_http.h>
#include <nginx.h>

#include <newrelic_collector_client.h>
#include <newrelic_common.h>
#include <newrelic_transaction.h>

typedef struct {
	long transaction_id;
	long parent_id;
	long segment_id;
} ngx_http_newrelic_transaction_state_t;


/* local copy of config */
static unsigned char *newrelic_license;
static unsigned char *app_name;
static unsigned char *app_lang;
static unsigned char *app_lang_ver;
static char *default_na_value = "N/A";


static void *ngx_http_newrelic_create_main_conf(ngx_conf_t *cf);
static char *ngx_http_newrelic_init_main_conf(ngx_conf_t *cf, void *conf);
static void *ngx_http_newrelic_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_newrelic_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static ngx_int_t ngx_http_newrelic_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_newrelic_process_init(ngx_cycle_t *cycle);
static void ngx_http_newrelic_process_exit(ngx_cycle_t *cycle);
static ngx_int_t ngx_http_newrelic_begin_request_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_newrelic_end_request_handler(ngx_http_request_t *r);

static ngx_command_t ngx_http_newrelic_commands[] = {
	{
		ngx_string("newrelic"),
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_FLAG,
		ngx_conf_set_flag_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(ngx_http_newrelic_loc_conf_t, enable),
		NULL
	},
	{
		ngx_string("newrelic_license_key"),
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_MAIN_CONF_OFFSET,
		offsetof(ngx_http_newrelic_main_conf_t, license_key),
		NULL
	},
	{
		ngx_string("newrelic_app_name"),
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_MAIN_CONF_OFFSET,
		offsetof(ngx_http_newrelic_main_conf_t, app_name),
		NULL
	},
	{
		ngx_string("newrelic_app_lang"),
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_MAIN_CONF_OFFSET,
		offsetof(ngx_http_newrelic_main_conf_t, app_lang),
		NULL
	},
	{
		ngx_string("newrelic_app_lang_ver"),
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_MAIN_CONF_OFFSET,
		offsetof(ngx_http_newrelic_main_conf_t, app_lang_ver),
		NULL
	},
	{
		ngx_string("newrelic_transaction_name"),
		NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(ngx_http_newrelic_loc_conf_t, transaction_name),
		NULL
	},
	ngx_null_command
};

static ngx_http_module_t ngx_http_newreilc_module_ctx = {
		NULL,                                       /* preconfiguration */
		ngx_http_newrelic_init,                     /* postconfiguration */
		ngx_http_newrelic_create_main_conf,         /* create main configuration */
		ngx_http_newrelic_init_main_conf,           /* init main configuration */
		NULL,                                       /* create server configuration */
		NULL,                                       /* merge server configuration */
		ngx_http_newrelic_create_loc_conf,          /* create location configuration */
		ngx_http_newrelic_merge_loc_conf            /* merge location configuration */
};

ngx_module_t ngx_http_newrelic_module = {
		NGX_MODULE_V1,
		&ngx_http_newreilc_module_ctx,              /* module context */
		ngx_http_newrelic_commands,                 /* module directives */
		NGX_HTTP_MODULE,                            /* module type */
		NULL,                                       /* init master */
		NULL,                                       /* init module */
		ngx_http_newrelic_process_init,             /* init process */
		NULL,                                       /* init thread */
		NULL,                                       /* exit thread */
		ngx_http_newrelic_process_exit,             /* exit process */
		NULL,                                       /* exit master */
		NGX_MODULE_V1_PADDING
};


static void *ngx_http_newrelic_create_main_conf(ngx_conf_t *cf) {
  /* ngx_log_error(NGX_LOG_NOTICE, cf->log, 0, "NR ngx_http_newrelic_create_main_conf"); */

	ngx_http_newrelic_main_conf_t *newrelic_main_conf;
	newrelic_main_conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_newrelic_main_conf_t));

	if (newrelic_main_conf == NULL) {
		return NULL; 
	}


	newrelic_main_conf->enable = NGX_CONF_UNSET;


	return newrelic_main_conf;
}

static char *ngx_http_newrelic_init_main_conf(ngx_conf_t *cf, void *conf) {
	ngx_http_newrelic_main_conf_t *newrelic_main_conf = conf;
	ngx_http_newrelic_loc_conf_t *newrelic_loc_conf =  ngx_http_conf_get_module_loc_conf(cf, ngx_http_newrelic_module);

	/* enable flag is saved in default loc conf - copy to main conf */
	if (newrelic_loc_conf->enable == NGX_CONF_UNSET) {
		newrelic_main_conf->enable = 0;
	} else {
	  newrelic_main_conf->enable = newrelic_loc_conf->enable; 
	}
	ngx_log_error(NGX_LOG_ERR, cf->log, 0, "NR ngx_http_newrelic_init_main_conf, %d", newrelic_main_conf->enable); 

	return NGX_CONF_OK;
}

static void *ngx_http_newrelic_create_loc_conf(ngx_conf_t *cf) {
	ngx_log_error(NGX_LOG_INFO, cf->log, 0, "NR ngx_http_newrelic_create_loc_conf"); 

	ngx_http_newrelic_loc_conf_t *newrelic_loc_conf;
	newrelic_loc_conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_newrelic_loc_conf_t));

	if (newrelic_loc_conf == NULL) {
		return NULL;
	}

	newrelic_loc_conf->enable = NGX_CONF_UNSET;
	//	newrelic_loc_conf->transaction_name.data = NULL;
	//newrelic_loc_conf->transaction_name.len = 0;

	return newrelic_loc_conf;
}

static char *ngx_http_newrelic_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child) {
	ngx_http_newrelic_loc_conf_t *prev = parent;
	ngx_http_newrelic_loc_conf_t *conf = child;

	ngx_http_newrelic_main_conf_t *main_conf =  ngx_http_conf_get_module_main_conf(cf, ngx_http_newrelic_module);

	/*	ngx_log_error(NGX_LOG_INFO, cf->log, 0, "NR ngx_http_newrelic_merge_loc_conf, %d, %d, main=%d, str len=%d", conf->enable, prev->enable, main_conf->enable, conf->transaction_name.len);  */

	if (conf->enable==1 && main_conf->enable==0) {
	  ngx_log_error(NGX_LOG_NOTICE, cf->log, 0, "newrelic is not enabled in main/server config, so cannot be enabled in location config");
	  return NGX_CONF_ERROR;
	}
	ngx_conf_merge_value(conf->enable, prev->enable, 0);

	return NGX_CONF_OK;
}

static ngx_int_t ngx_http_newrelic_init(ngx_conf_t *cf) {
	ngx_http_handler_pt *begin_request_handler;
	ngx_http_handler_pt *end_request_handler;
	ngx_http_core_main_conf_t *core_main_conf;
	ngx_http_newrelic_main_conf_t *newrelic_conf;
	
	newrelic_conf = ngx_http_conf_get_module_main_conf(cf, ngx_http_newrelic_module);
	
	if (!newrelic_conf->enable) {
	  ngx_log_error(NGX_LOG_NOTICE, cf->log, 0, "newrelic is not enabled");
	  return NGX_OK;
	}

	if (!newrelic_conf->license_key.data) {
	  ngx_log_error(NGX_LOG_NOTICE, cf->log, 0, "newrelic license key is nil");
	  return NGX_OK;
	}

	if (!newrelic_conf->app_name.data) {
	  ngx_log_error(NGX_LOG_NOTICE, cf->log, 0, "newrelic app name is nil");
	  return NGX_OK;
	}

	newrelic_license = newrelic_conf->license_key.data;
	app_name = newrelic_conf->app_name.data;
	app_lang = newrelic_conf->app_lang.data;
	if (app_lang==0) app_lang = (unsigned char *)default_na_value;
	app_lang_ver = newrelic_conf->app_lang_ver.data;
	if (app_lang_ver==0) app_lang_ver = (unsigned char*)default_na_value;

	core_main_conf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

	//begin_request_handler = ngx_array_push(&core_main_conf->phases[NGX_HTTP_POST_READ_PHASE].handlers);
	begin_request_handler = ngx_array_push(&core_main_conf->phases[NGX_HTTP_PREACCESS_PHASE].handlers);
	end_request_handler = ngx_array_push(&core_main_conf->phases[NGX_HTTP_LOG_PHASE].handlers);

	if (begin_request_handler == NULL) {
		return NGX_ERROR;
	}

	if (begin_request_handler == NULL) {
		return NGX_ERROR;
	}

	*begin_request_handler = ngx_http_newrelic_begin_request_handler;
	*end_request_handler = ngx_http_newrelic_end_request_handler;

	return NGX_OK;
}

static ngx_int_t ngx_http_newrelic_process_init(ngx_cycle_t *cycle) {
  /* ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0, "NR ngx_http_newrelic_init_process");  */

	ngx_http_newrelic_main_conf_t *newrelic_main_conf;

	newrelic_main_conf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_newrelic_module);

	if (!newrelic_main_conf->enable) {
	  /*	  ngx_log_error(NGX_LOG_ERR, cycle->log, 0, "NR ngx_http_newrelic_process_init, not enabled");*/
		return NGX_OK;
	}

	/* ngx_log_error(NGX_LOG_ERR, cycle->log, 0, "NR ngx_http_newrelic_process_init, enabled");*/

	newrelic_register_message_handler(newrelic_message_handler);

	newrelic_init((char *)newrelic_license, (char*) app_name, (char *)app_lang, (char *)app_lang_ver);
	/* ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0, "NR ngx_http_newrelic_init_process, nr_error_code = %d", nr_error_code); */
	return NGX_OK;
}

static void ngx_http_newrelic_process_exit(ngx_cycle_t *cycle) {
  /*	ngx_log_error(NGX_LOG_NOTICE, cycle->log, 0, "NR ngx_http_newrelic_exit_process");*/

	ngx_http_newrelic_main_conf_t *newrelic_conf;

	newrelic_conf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_newrelic_module);

	if (!newrelic_conf->enable) {
		return;
	}
}

static ngx_int_t ngx_http_newrelic_begin_request_handler(ngx_http_request_t *r) {
  ngx_http_newrelic_loc_conf_t *newrelic_loc_conf = ngx_http_get_module_loc_conf(r, ngx_http_newrelic_module);

  if (!newrelic_loc_conf->enable) {
    /*	ngx_log_error(NGX_LOG_NOTICE, r->connection->log, 0, "NR not enableed for this location");*/
    return NGX_OK;
  }
  
  long transaction_id = newrelic_transaction_begin();

  /*  set name - either newrelic_transaction_name from location conf, or current uri (which is NOT null-terminated BTW!) */
	char* transaction_name;
	if (newrelic_loc_conf->transaction_name.data) {
	  transaction_name = (char *) newrelic_loc_conf->transaction_name.data;
	} else {
	  transaction_name = ngx_pcalloc(r->pool, r->uri.len+1);
	  ngx_cpymem(transaction_name, r->uri.data, r->uri.len);
	  transaction_name[r->uri.len] = 0;	  
	}
	newrelic_transaction_set_name(transaction_id, transaction_name);

	/* create context state for this request */
	ngx_http_newrelic_transaction_state_t* ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_newrelic_transaction_state_t));

	if (ctx == NULL) {
	  ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "NR cannot allocate memory for ctx");
	  return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}
	ctx->transaction_id = transaction_id;
	ctx->parent_id = ctx->segment_id = 0;

	/* save context in request */
	ngx_http_set_ctx(r, ctx, ngx_http_newrelic_module);

	/*ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "NR ngx_http_newrelic_begin_request_handler, name=%s, tid=%ld", transaction_name, transaction_id);*/
  
	return NGX_OK;
}

static ngx_int_t ngx_http_newrelic_end_request_handler(ngx_http_request_t *r) {

  ngx_http_newrelic_transaction_state_t* ctx = ngx_http_get_module_ctx(r, ngx_http_newrelic_module);
  if (ctx != 0) {
    newrelic_transaction_end(ctx->transaction_id);
    /*    ngx_log_error(NGX_LOG_NOTICE, r->connection->log, 0, "NR ngx_http_newrelic_end_request_handler, tid=%ld, rc=%d", ctx->transaction_id, return_code); */

  } else {
    /* this is normal - it means the location does not have newrelic on; */
    /*  ngx_log_error(NGX_LOG_NOTICE, r->connection->log, 0, "NR ngx_http_newrelic_end_request_handler, no CTX found!"); */
  }
  return NGX_OK;
}
