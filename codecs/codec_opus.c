/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2014, Lorenzo Miniero
 *
 * Lorenzo Miniero <lorenzo@meetecho.com>
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*! \file
 *
 * \brief Translate between signed linear and Opus (Open Codec)
 *
 * \author Lorenzo Miniero <lorenzo@meetecho.com>
 *
 * \note This work was motivated by Mozilla
 *
 * \ingroup codecs
 *
 * \extref The Opus library - http://opus-codec.org
 *
 */

/*** MODULEINFO
	 <depend>opus</depend>
	 <support_level>core</support_level>
***/

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision: $")

#include <opus/opus.h>

#include "asterisk/translate.h"
#include "asterisk/module.h"
#include "asterisk/config.h"
#include "asterisk/utils.h"

#define	BUFFER_SAMPLES	8000
#define	OPUS_SAMPLES	160

#define USE_FEC			0

/* Sample frame data */
#include "asterisk/slin.h"
#include "ex_opus.h"

/* FIXME: Test */
#include "asterisk/file.h"

static int encid;
static int decid;

static int opusdebug;

/* Private structures */
struct opus_coder_pvt {
	void *opus;	/* May be encoder or decoder */
	int sampling_rate;
	int multiplier;
	int fec;
	int id;
	int16_t buf[BUFFER_SAMPLES];	/* FIXME */
	int framesize;
};

static int valid_sampling_rate(int rate)
{
	return rate == 8000
		|| rate == 12000
		|| rate == 16000
		|| rate == 24000
		|| rate == 48000;
}

/* Helper methods */
static int opus_encoder_construct(struct ast_trans_pvt *pvt, int sampling_rate)
{
	struct opus_coder_pvt *opvt = pvt->pvt;
	int error = 0;

	if (!valid_sampling_rate(sampling_rate)) {
		return -1;
	}

	opvt->sampling_rate = sampling_rate;
	opvt->multiplier = 48000/sampling_rate;
	opvt->fec = USE_FEC;

	opvt->opus = opus_encoder_create(sampling_rate, 1, OPUS_APPLICATION_VOIP, &error);

	if (error != OPUS_OK) {
		ast_log(LOG_ERROR, "Error creating the Opus encoder: %s\n", opus_strerror(error));
		return -1;
	}

	if (sampling_rate == 8000) {
		opus_encoder_ctl(opvt->opus, OPUS_SET_MAX_BANDWIDTH(OPUS_BANDWIDTH_NARROWBAND));
	} else if(sampling_rate == 12000) {
		opus_encoder_ctl(opvt->opus, OPUS_SET_MAX_BANDWIDTH(OPUS_BANDWIDTH_MEDIUMBAND));
	} else if(sampling_rate == 16000) {
		opus_encoder_ctl(opvt->opus, OPUS_SET_MAX_BANDWIDTH(OPUS_BANDWIDTH_WIDEBAND));
	} else if(sampling_rate == 24000) {
		opus_encoder_ctl(opvt->opus, OPUS_SET_MAX_BANDWIDTH(OPUS_BANDWIDTH_SUPERWIDEBAND));
	} else if(sampling_rate == 48000) {
		opus_encoder_ctl(opvt->opus, OPUS_SET_MAX_BANDWIDTH(OPUS_BANDWIDTH_FULLBAND));
	}

	opus_encoder_ctl(opvt->opus, OPUS_SET_INBAND_FEC(opvt->fec));
	opvt->framesize = sampling_rate/50;
	opvt->id = ++encid;

	ast_debug(3, "Created encoder #%d (%d->opus)\n", opvt->id, sampling_rate);

	return 0;
}

static int opus_decoder_construct(struct ast_trans_pvt *pvt, int sampling_rate)
{
	struct opus_coder_pvt *opvt = pvt->pvt;
	int error = 0;

	if (!valid_sampling_rate(sampling_rate)) {
		return -1;
	}

	opvt->sampling_rate = sampling_rate;
	opvt->multiplier = 48000/sampling_rate;
	opvt->fec = USE_FEC;	/* FIXME: should be triggered by chan_sip */

	opvt->opus = opus_decoder_create(sampling_rate, 1, &error);

	if (error != OPUS_OK) {
		ast_log(LOG_ERROR, "Error creating the Opus decoder: %s\n", opus_strerror(error));
		return -1;
	}

	opvt->id = ++decid;

	ast_debug(3, "Created decoder #%d (opus->%d)\n", opvt->id, sampling_rate);

	return 0;
}

/* Translator callbacks */
static int lintoopus_new(struct ast_trans_pvt *pvt)
{
	return opus_encoder_construct(pvt, 8000);
}

static int lin12toopus_new(struct ast_trans_pvt *pvt)
{
	return opus_encoder_construct(pvt, 12000);
}

static int lin16toopus_new(struct ast_trans_pvt *pvt)
{
	return opus_encoder_construct(pvt, 16000);
}

static int lin24toopus_new(struct ast_trans_pvt *pvt)
{
	return opus_encoder_construct(pvt, 24000);
}

static int lin48toopus_new(struct ast_trans_pvt *pvt)
{
	return opus_encoder_construct(pvt, 48000);
}

static int opustolin_new(struct ast_trans_pvt *pvt)
{
	return opus_decoder_construct(pvt, 8000);
}

static int opustolin12_new(struct ast_trans_pvt *pvt)
{
	return opus_decoder_construct(pvt, 12000);
}

static int opustolin16_new(struct ast_trans_pvt *pvt)
{
	return opus_decoder_construct(pvt, 16000);
}

static int opustolin24_new(struct ast_trans_pvt *pvt)
{
	return opus_decoder_construct(pvt, 24000);
}

static int opustolin48_new(struct ast_trans_pvt *pvt)
{
	return opus_decoder_construct(pvt, 48000);
}

static int lintoopus_framein(struct ast_trans_pvt *pvt, struct ast_frame *f)
{
	struct opus_coder_pvt *opvt = pvt->pvt;

	/* XXX We should look at how old the rest of our stream is, and if it
	   is too old, then we should overwrite it entirely, otherwise we can
	   get artifacts of earlier talk that do not belong */
	memcpy(opvt->buf + pvt->samples, f->data.ptr, f->datalen);
	pvt->samples += f->samples;

	return 0;
}

static struct ast_frame *lintoopus_frameout(struct ast_trans_pvt *pvt)
{
	struct opus_coder_pvt *opvt = pvt->pvt;

	/* We can't work on anything less than a frame in size */
	if (pvt->samples < opvt->framesize) {
		return NULL;
	}

	int datalen = 0;	/* output bytes */
	int samples = 0;	/* output samples */

	/* Encode 160 samples (or more if it's not narrowband) */
	ast_debug(3, "[Encoder #%d (%d)] %d samples, %d bytes\n",
		opvt->id,
		opvt->sampling_rate,
		opvt->framesize,
		opvt->framesize * 2);

	datalen = opus_encode(opvt->opus, opvt->buf, opvt->framesize, pvt->outbuf.uc, BUFFER_SAMPLES);

	if (datalen < 0) {
		ast_log(LOG_ERROR, "Error encoding the Opus frame: %s\n", opus_strerror(datalen));
		return NULL;
	}

	samples += opvt->framesize;
	pvt->samples -= opvt->framesize;

	/* Move the data at the end of the buffer to the front */
	if (pvt->samples) {
		memmove(opvt->buf, opvt->buf + samples, pvt->samples * 2);
	}

	ast_debug(3, "[Encoder #%d (%d)]   >> Got %d samples, %d bytes\n",
		opvt->id,
		opvt->sampling_rate,
		opvt->multiplier * samples,
		datalen);

	return ast_trans_frameout(pvt, datalen, opvt->multiplier * samples);
}

static int opustolin_framein(struct ast_trans_pvt *pvt, struct ast_frame *f)
{
	struct opus_coder_pvt *opvt = pvt->pvt;
	int error = 0;

	/* Decode */
	ast_debug(3, "[Decoder #%d (%d)] %d samples, %d bytes\n",
		opvt->id,
		opvt->sampling_rate,
		f->samples,
		f->datalen);

	error = opus_decode(opvt->opus, f->data.ptr, f->datalen, pvt->outbuf.i16, BUFFER_SAMPLES, opvt->fec);

	if (error < 0) {
		ast_log(LOG_ERROR, "Error decoding the Opus frame: %s\n", opus_strerror(error));
		return -1;
	}

	pvt->samples += error;
	pvt->datalen += error * 2;

	ast_debug(3, "[Decoder #%d (%d)]   >> Got %d samples, %d bytes\n",
		opvt->id,
		opvt->sampling_rate,
		pvt->samples,
		pvt->datalen);

	return 0;
}

static void lintoopus_destroy(struct ast_trans_pvt *arg)
{
	struct opus_coder_pvt *opvt = arg->pvt;

	if (!opvt || !opvt->opus) {
		return;
	}

	opus_encoder_destroy(opvt->opus);
	opvt->opus = NULL;

	ast_debug(3, "Destroyed encoder #%d (%d->opus)\n", opvt->id, opvt->sampling_rate);
}

static void opustolin_destroy(struct ast_trans_pvt *arg)
{
	struct opus_coder_pvt *opvt = arg->pvt;

	if (!opvt || !opvt->opus) {
		return;
	}

	opus_decoder_destroy(opvt->opus);
	opvt->opus = NULL;

	ast_debug(3, "Destroyed decoder #%d (opus->%d)\n", opvt->id, opvt->sampling_rate);
}

/* Translators */
static struct ast_translator lintoopus = {
	.name = "lintoopus",
	.newpvt = lintoopus_new,
	.framein = lintoopus_framein,
	.frameout = lintoopus_frameout,
	.destroy = lintoopus_destroy,
	.sample = slin8_sample,
	.desc_size = sizeof(struct opus_coder_pvt),
	.buffer_samples = BUFFER_SAMPLES,
	.buf_size = BUFFER_SAMPLES * 2,
};

static struct ast_translator lin12toopus = {
	.name = "lin12toopus",
	.newpvt = lin12toopus_new,
	.framein = lintoopus_framein,
	.frameout = lintoopus_frameout,
	.destroy = lintoopus_destroy,
	.desc_size = sizeof(struct opus_coder_pvt),
	.buffer_samples = BUFFER_SAMPLES,
	.buf_size = BUFFER_SAMPLES * 2,
};

static struct ast_translator lin16toopus = {
	.name = "lin16toopus",
	.newpvt = lin16toopus_new,
	.framein = lintoopus_framein,
	.frameout = lintoopus_frameout,
	.destroy = lintoopus_destroy,
	.sample = slin16_sample,
	.desc_size = sizeof(struct opus_coder_pvt),
	.buffer_samples = BUFFER_SAMPLES,
	.buf_size = BUFFER_SAMPLES * 2,
};

static struct ast_translator lin24toopus = {
	.name = "lin24toopus",
	.newpvt = lin24toopus_new,
	.framein = lintoopus_framein,
	.frameout = lintoopus_frameout,
	.destroy = lintoopus_destroy,
	.desc_size = sizeof(struct opus_coder_pvt),
	.buffer_samples = BUFFER_SAMPLES,
	.buf_size = BUFFER_SAMPLES * 2,
};

static struct ast_translator lin48toopus = {
	.name = "lin48toopus",
	.newpvt = lin48toopus_new,
	.framein = lintoopus_framein,
	.frameout = lintoopus_frameout,
	.destroy = lintoopus_destroy,
	.desc_size = sizeof(struct opus_coder_pvt),
	.buffer_samples = BUFFER_SAMPLES,
	.buf_size = BUFFER_SAMPLES * 2,
};

static struct ast_translator opustolin = {
	.name = "opustolin",
	.newpvt = opustolin_new,
	.framein = opustolin_framein,
	.destroy = opustolin_destroy,
	.sample = opus_sample,
	.desc_size = sizeof(struct opus_coder_pvt),
	.buffer_samples = BUFFER_SAMPLES,
	.buf_size = BUFFER_SAMPLES * 2,
	.native_plc = 1,	/* FIXME: needed? */
};

static struct ast_translator opustolin12 = {
	.name = "opustolin12",
	.newpvt = opustolin12_new,
	.framein = opustolin_framein,
	.destroy = opustolin_destroy,
	.sample = opus_sample,
	.desc_size = sizeof(struct opus_coder_pvt),
	.buffer_samples = BUFFER_SAMPLES,
	.buf_size = BUFFER_SAMPLES * 2,
	.native_plc = 1,	/* FIXME: needed? */
};

static struct ast_translator opustolin16 = {
	.name = "opustolin16",
	.newpvt = opustolin16_new,
	.framein = opustolin_framein,
	.destroy = opustolin_destroy,
	.sample = opus_sample,
	.desc_size = sizeof(struct opus_coder_pvt),
	.buffer_samples = BUFFER_SAMPLES,
	.buf_size = BUFFER_SAMPLES * 2,
	.native_plc = 1,	/* FIXME: needed? */
};

static struct ast_translator opustolin24 = {
	.name = "opustolin24",
	.newpvt = opustolin24_new,
	.framein = opustolin_framein,
	.destroy = opustolin_destroy,
	.sample = opus_sample,
	.desc_size = sizeof(struct opus_coder_pvt),
	.buffer_samples = BUFFER_SAMPLES,
	.buf_size = BUFFER_SAMPLES * 2,
	.native_plc = 1,	/* FIXME: needed? */
};

static struct ast_translator opustolin48 = {
	.name = "opustolin48",
	.newpvt = opustolin48_new,
	.framein = opustolin_framein,
	.destroy = opustolin_destroy,
	.sample = opus_sample,
	.desc_size = sizeof(struct opus_coder_pvt),
	.buffer_samples = BUFFER_SAMPLES,
	.buf_size = BUFFER_SAMPLES * 2,
	.native_plc = 1,	/* FIXME: needed? */
};

/* Configuration and module setup */
static int parse_config(int reload)
{
	/* TODO: place stuff to negotiate/enforce here */
	return 0;
}

static int reload(void)
{
	if (parse_config(1)) {
		return AST_MODULE_LOAD_DECLINE;
	}

	return AST_MODULE_LOAD_SUCCESS;
}

static int unload_module(void)
{
	int res = 0;

	res |= ast_unregister_translator(&opustolin);
	res |= ast_unregister_translator(&lintoopus);
	res |= ast_unregister_translator(&opustolin12);
	res |= ast_unregister_translator(&lin12toopus);
	res |= ast_unregister_translator(&opustolin16);
	res |= ast_unregister_translator(&lin16toopus);
	res |= ast_unregister_translator(&opustolin24);
	res |= ast_unregister_translator(&lin24toopus);
	res |= ast_unregister_translator(&opustolin48);
	res |= ast_unregister_translator(&lin48toopus);

	return res;
}

static int load_module(void)
{
	int res = 0;

	if (parse_config(0)) {
		return AST_MODULE_LOAD_DECLINE;
	}

	/* 8khz (nb) */
	ast_format_set(&opustolin.src_format, AST_FORMAT_OPUS, 0);
	ast_format_set(&opustolin.dst_format, AST_FORMAT_SLINEAR, 0);
	ast_format_set(&lintoopus.src_format, AST_FORMAT_SLINEAR, 0);
	ast_format_set(&lintoopus.dst_format, AST_FORMAT_OPUS, 0);

	/* 12khz (mb) */
	ast_format_set(&opustolin12.src_format, AST_FORMAT_OPUS, 0);
	ast_format_set(&opustolin12.dst_format, AST_FORMAT_SLINEAR12, 0);
	ast_format_set(&lin12toopus.src_format, AST_FORMAT_SLINEAR12, 0);
	ast_format_set(&lin12toopus.dst_format, AST_FORMAT_OPUS, 0);

	/* 16khz (wb) */
	ast_format_set(&opustolin16.src_format, AST_FORMAT_OPUS, 0);
	ast_format_set(&opustolin16.dst_format, AST_FORMAT_SLINEAR16, 0);
	ast_format_set(&lin16toopus.src_format, AST_FORMAT_SLINEAR16, 0);
	ast_format_set(&lin16toopus.dst_format, AST_FORMAT_OPUS, 0);

	/* 24khz (swb) */
	ast_format_set(&opustolin24.src_format, AST_FORMAT_OPUS, 0);
	ast_format_set(&opustolin24.dst_format, AST_FORMAT_SLINEAR24, 0);
	ast_format_set(&lin24toopus.src_format, AST_FORMAT_SLINEAR24, 0);
	ast_format_set(&lin24toopus.dst_format, AST_FORMAT_OPUS, 0);

	/* 48khz (fb) */
	ast_format_set(&opustolin48.src_format, AST_FORMAT_OPUS, 0);
	ast_format_set(&opustolin48.dst_format, AST_FORMAT_SLINEAR48, 0);
	ast_format_set(&lin48toopus.src_format, AST_FORMAT_SLINEAR48, 0);
	ast_format_set(&lin48toopus.dst_format, AST_FORMAT_OPUS, 0);

	res |= ast_register_translator(&opustolin);
	res |= ast_register_translator(&lintoopus);
	res |= ast_register_translator(&opustolin12);
	res |= ast_register_translator(&lin12toopus);
	res |= ast_register_translator(&opustolin16);
	res |= ast_register_translator(&lin16toopus);
	res |= ast_register_translator(&opustolin24);
	res |= ast_register_translator(&lin24toopus);
	res |= ast_register_translator(&opustolin48);
	res |= ast_register_translator(&lin48toopus);

	return res;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "Opus Coder/Decoder",
	.load = load_module,
	.unload = unload_module,
	.reload = reload,
	);
