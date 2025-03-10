#include <string>

#include <cstdio>
#include <cstring>
#include <cassert>

#include <sys/select.h>

#include <sphinxbase/err.h>
#include <sphinxbase/ad.h>

#include "pocketsphinx.h"

using namespace std;

static const arg_t cont_args_def[] = {
				POCKETSPHINX_OPTIONS,
				/* Argument file. */
				{"-argfile",
								ARG_STRING,
								NULL,
								"Argument file giving extra arguments."},
				{"-adcdev",
								ARG_STRING,
								NULL,
								"Name of audio device to use for input."},
				{"-infile",
								ARG_STRING,
								NULL,
								"Audio file to transcribe."},
				{"-inmic",
								ARG_BOOLEAN,
								"yes",
								"Transcribe audio from microphone."},
				{"-time",
								ARG_BOOLEAN,
								"no",
								"Print word times in file transcription."},
				CMDLN_EMPTY_OPTION
};

static ps_decoder_t *ps;
static cmd_ln_t *config;
static FILE *rawfd;

static void print_word_times()
{
				int frame_rate = cmd_ln_int32_r(config, "-frate");
				ps_seg_t *iter = ps_seg_iter(ps);
				while (iter != NULL) {
								int32 sf, ef, pprob;
								float conf;

								ps_seg_frames(iter, &sf, &ef);
								pprob = ps_seg_prob(iter, NULL, NULL, NULL);
								conf = logmath_exp(ps_get_logmath(ps), pprob);
								printf("%s %.3f %.3f %f\n", ps_seg_word(iter), ((float)sf / frame_rate),
																((float) ef / frame_rate), conf);
								iter = ps_seg_next(iter);
				}
}

static int check_wav_header(char *header, int expected_sr)
{
				int sr;

				if (header[34] != 0x10) {
								E_ERROR("Input audio file has [%d] bits per sample instead of 16\n", header[34]);
								return 0;
				}
				if (header[20] != 0x1) {
								E_ERROR("Input audio file has compression [%d] and not required PCM\n", header[20]);
								return 0;
				}
				if (header[22] != 0x1) {
								E_ERROR("Input audio file has [%d] channels, expected single channel mono\n", header[22]);
								return 0;
				}
				sr = ((header[24] & 0xFF) | ((header[25] & 0xFF) << 8) | ((header[26] & 0xFF) << 16) | ((header[27] & 0xFF) << 24));
				if (sr != expected_sr) {
								E_ERROR("Input audio file has sample rate [%d], but decoder expects [%d]\n", sr, expected_sr);
								return 0;
				}
				return 1;
}

static void sleep_msec(int32 ms)
{
				/* ------------------- Unix ------------------ */
				struct timeval tmo;

				tmo.tv_sec = 0;
				tmo.tv_usec = ms * 1000;

				select(0, NULL, NULL, NULL, &tmo);
}

/*
 * Main utterance processing loop:
 *     for (;;) {
 *        start utterance and wait for speech to process
 *        decoding till end-of-utterance silence will be detected
 *        print utterance result;
 *     }
 */
static void recognize_from_microphone(string desired_command)
{
				ad_rec_t *ad;
				int16 adbuf[2048];
				uint8 utt_started, in_speech;
				int32 k;
				char const *hyp;

				if ((ad = ad_open_dev(cmd_ln_str_r(config, "-adcdev"),
																				(int) cmd_ln_float32_r(config,
																								"-samprate"))) == NULL)
								E_FATAL("Failed to open audio device\n");
				if (ad_start_rec(ad) < 0)
								E_FATAL("Failed to start recording\n");

				if (ps_start_utt(ps) < 0)
								E_FATAL("Failed to start utterance\n");
				utt_started = FALSE;
				E_INFO("Ready....\n");

				// Control listening.
				while(true){
									if ((k = ad_read(ad, adbuf, 2048)) < 0){
													E_FATAL("Failed to read audio\n");
									}
									ps_process_raw(ps, adbuf, k, FALSE, FALSE);
									in_speech = ps_get_in_speech(ps);
	
									// If have someone talking and not statarted to hear. So start 
									// listening.
									if (in_speech && !utt_started) {
													utt_started = TRUE;
													E_INFO("Listening...\n");
									}
	
									// If no one is talking and is hearing. So stop it, and print the 
									// result.
									if (!in_speech && utt_started) {
													/* speech -> silence transition, time to start new utterance  */
													ps_end_utt(ps);
													hyp = ps_get_hyp(ps, NULL );
													// If uderstand somethig. So print it.
													if (hyp != NULL) {
																	// Print the value recognized.
																	printf("\n\n\n\n\n");
																	printf("%s\n", hyp);
																	if(!strcmp(hyp, desired_command.c_str())){ 
																		printf("\n\n\n\n\n");
																		printf("Stop listening\n");
																		exit(1);
																	}
																	printf("\n\n\n\n\n");
																	fflush(stdout);
													}
	
													if (ps_start_utt(ps) < 0)
																	E_FATAL("Failed to start utterance\n");
													utt_started = FALSE;
													E_INFO("Ready....\n");
									}
								sleep_msec(100);
				}
				ad_close(ad);
}

/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 1999-2010 Carnegie Mellon University.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * This work was supported in part by funding from the Defense Advanced 
 * Research Projects Agency and the National Science Foundation of the 
 * United States of America, and the CMU Sphinx Speech Consortium.
 *
 * THIS SOFTWARE IS PROVIDED BY CARNEGIE MELLON UNIVERSITY ``AS IS'' AND 
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
 * NOR ITS EMPLOYEES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ====================================================================
 *
 */
/*
 * continuous.c - Simple pocketsphinx command-line application to test
 *                both continuous listening/silence filtering from microphone
 *                and continuous file transcription.
 */

/*
 * This is a simple example of pocketsphinx application that uses continuous listening
 * with silence filtering to automatically segment a continuous stream of audio input
 * into utterances that are then decoded.
 * 
 * Remarks:
 *   - Each utterance is ended when a silence segment of at least 1 sec is recognized.
 *   - Single-threaded implementation for portability.
 *   - Uses audio library; can be replaced with an equivalent custom library.
 */
