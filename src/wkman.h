#ifndef WKMAN_H
#define WKMAN_H 1

struct device;
struct wkman;
struct cfg;

enum {
	WKM_RESULT_OK,
	WKM_RESULT_DEVICE_OFFLINE,
	WKM_RESULT_TIMEOUT,
	WKM_RESULT_IO_ERROR,
	WKM_RESULT_BUSY,
};

typedef void (*wkm_completion_cb)(int result, void *user_data);

struct wkman *wkm_create(struct ev_loop *loop, struct device *dev);
void wkm_destroy(struct wkman *wkman);
int wkm_set_value(struct wkman *wkman, const char *key, uint8_t val);
int wkm_foreach(struct wkman *wkman, int (*cb)(const char *key, int val, void *user_data), void *user_data);
int wkm_opts_to_cfg(struct wkman *wkman, struct cfg *cfg);
int wkm_cfg_to_opts(struct wkman *wkman, struct cfg *base_hdf);
int wkm_reset(struct wkman *wkman);
int wkm_host_open(struct wkman *wkman);
int wkm_host_close(struct wkman *wkman);
int wkm_read_cfg(struct wkman *wkman, wkm_completion_cb cb, void *user_data);
int wkm_write_cfg(struct wkman *wkman);
const char *wkm_err_string(int error);


#endif /* WKMAN_H */
