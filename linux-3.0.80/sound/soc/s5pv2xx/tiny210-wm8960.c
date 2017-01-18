/*
* mini_wm8960.c  --  WM8960 ALSA SoC Audio driver
*
* Author: happyzlz
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/

#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include "s5pc1xx-i2s.h"
#include "wm8960.h"


#define    CDCLK                    0x002
#define    CLKAUDIO                0x400


static struct snd_soc_card tiny210_soc_card;

static int tiny210_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
    struct snd_soc_pcm_runtime *rtd = substream->private_data;
    struct snd_soc_dai *codec_dai = rtd->codec_dai;
    struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
    unsigned int rate = params_rate(params);
    snd_pcm_format_t fmt = params_format( params );
    int    ret = 0;

    int div;
    int r9;
    int sl;    
    unsigned int mul;
    unsigned int clk_scale;

    switch ( fmt ) {
        case SNDRV_PCM_FORMAT_S16:
            div = 32;
            break;
        case SNDRV_PCM_FORMAT_S20_3LE:
        case SNDRV_PCM_FORMAT_S24:
            div = 48;
            break;
        default:
            return -EINVAL;
    }
    
    switch ( rate ) {
        case 8000:
        case 11025:
        case 12000:
            if( div == 48 )
                r9 = 768;    //0x300
            else
                r9 = 512;    //0x200
            break;

        case 16000: 
        case 22050: 
        case 24000:
        case 32000:
        case 44100:    //AC44
        case 48000: 
        case 88200:
        case 96000:
            if( div == 48 )
                r9 = 384;    //0x180
            else
                r9 = 256;    //0x100
            break;

        case 64000:
            r9 = 384;        //0x180
            break;

        default:
            return -EINVAL;
    }
    
    ret = snd_soc_dai_set_fmt( codec_dai, SND_SOC_DAIFMT_I2S|SND_SOC_DAIFMT_NB_NF|SND_SOC_DAIFMT_CBS_CFS );
    if( ret < 0 ){
        printk( "%s: Codec DAI configuration error, %d\n", __func__, ret );
        return ret;
    }

    ret = snd_soc_dai_set_fmt( cpu_dai, SND_SOC_DAIFMT_I2S|SND_SOC_DAIFMT_NB_NF|SND_SOC_DAIFMT_CBS_CFS );
    if( ret < 0 ){
        printk( "%s: AP DAI configuration error, %d\n", __func__, ret );
        return ret;
    }

    ret = snd_soc_dai_set_sysclk( cpu_dai, CDCLK, rate, SND_SOC_CLOCK_OUT );    //44100
    if( ret < 0 ){
        printk( "%s: AP sycclk CDCLK setting error, %d\n", __func__, ret );
        return ret;
    }

    ret = snd_soc_dai_set_sysclk( cpu_dai, CLKAUDIO, rate, SND_SOC_CLOCK_OUT );    //44100
    if( ret < 0 ){
        printk( "%s: AP sysclk CLKAUDIO setting error, %d\n", __func__, ret );
        return ret;
    }

    if( rate & 0x0F )
        sl = 0x04099990;
    else
        sl = 0x02ee0000;

    ret = snd_soc_dai_set_clkdiv( codec_dai, WM8960_SYSCLKDIV, WM8960_SYSCLK_DIV_1 );
    if( ret < 0 ){
        printk( "%s: Codec SYSCLKDIV setting error, %d\n", __func__, ret );
        return ret;
    }

    ret = snd_soc_dai_set_clkdiv( codec_dai, WM8960_DACDIV, WM8960_DAC_DIV_1  );
    if( ret < 0 ){
        printk( "%s: Codec DACDIV setting error, %d\n", __func__, ret );
        return ret;
    }

    mul = rate * r9;
    ret = snd_soc_dai_set_clkdiv( codec_dai, WM8960_DCLKDIV, mul );
    if( ret < 0 ){
        printk( "%s: Codec DCLKDIV setting error, %d\n", __func__, ret );
        return ret;
    }
    
    clk_scale =  sl / mul;
    clk_scale--;
    ret = snd_soc_dai_set_clkdiv( cpu_dai, S3C64XX_DIV_PRESCALER, clk_scale );
    if( ret < 0 ){
        printk( "%s: AP prescalar setting error, %d\n", __func__, ret );
        return ret;
    }

    ret = snd_soc_dai_set_clkdiv( cpu_dai, S3C64XX_DIV_RCLK, r9 );
    if( ret < 0 ){
        printk( "%s: AP RFS setting error, %d\n", __func__, ret );
        return ret;
    }

    ret = snd_soc_dai_set_clkdiv( cpu_dai, S3C64XX_DIV_BCLK, div );
    if( ret < 0 ){
        printk( "%s: AP BFS setting error, %d\n", __func__, ret );
        return ret;
    }
// rate = 44100 = 0xAC44
// div == 32
// sl = 0x04099990 = 67738000
// r9 = 256 = 0x100
// mul ==  r9 * rate = 11289600 = 0xAC4400
// clk_scale == sl /  mul - 1 = 5
    return 0;
}

static struct snd_soc_ops tiny210_wm8960_ops = {
    .hw_params = tiny210_hw_params,
};

static const struct snd_soc_dapm_widget tiny210_dapm_capture_widgets[] = {
    SND_SOC_DAPM_MIC(    "Mic Jack",            NULL ),
    SND_SOC_DAPM_LINE(    "Line Input 3 (FM)",NULL ),
};

static const struct snd_soc_dapm_widget tiny210_dapm_playback_widgets[] = {
    SND_SOC_DAPM_HP(    "Headphone Jack",    NULL ),
    SND_SOC_DAPM_SPK(    "Speaker_L",        NULL ),
    SND_SOC_DAPM_SPK(    "Speaker_R",        NULL ),
};

static const struct snd_soc_dapm_route tiny210_audio_map[] = {
    { "Headphone Jack",    NULL,    "HP_L"        },
    { "Headphone Jack",    NULL,     "HP_R"        },
    { "Speaker_L",        NULL,     "SPK_LP"    }, 
    { "Speaker_L",        NULL,     "SPK_LN"     }, 
    { "Speaker_R",        NULL,     "SPK_RP"     }, 
    { "Speaker_R",        NULL,     "SPK_RN"     }, 
    { "LINPUT1",        NULL,     "MICB"        },
    { "MICB",            NULL,     "Mic Jack"    },
};

static int tiny210_wm8960_init(struct snd_soc_pcm_runtime *rtd)
{
    struct snd_soc_codec *codec = rtd->codec;
    struct snd_soc_dapm_context *dapm = &codec->dapm;

    snd_soc_dapm_nc_pin(dapm, "RINPUT1");
    snd_soc_dapm_nc_pin(dapm, "LINPUT2");
    snd_soc_dapm_nc_pin(dapm, "RINPUT2");
    snd_soc_dapm_nc_pin(dapm, "OUT3");
    
    snd_soc_dapm_new_controls( dapm, tiny210_dapm_capture_widgets, ARRAY_SIZE( tiny210_dapm_capture_widgets ) );
    snd_soc_dapm_new_controls( dapm, tiny210_dapm_playback_widgets, ARRAY_SIZE( tiny210_dapm_playback_widgets ) );

    snd_soc_dapm_add_routes( dapm, tiny210_audio_map, ARRAY_SIZE( tiny210_audio_map ) );

    snd_soc_dapm_enable_pin(dapm, "Headphone Jack");
    snd_soc_dapm_enable_pin(dapm, "Mic Jack");
    snd_soc_dapm_enable_pin(dapm, "Speaker_L");
    snd_soc_dapm_enable_pin(dapm, "Speaker_R");
    
    snd_soc_dapm_disable_pin(dapm, "Line Input 3 (FM)");

    snd_soc_dapm_sync( dapm );

    return 0;
}

static struct snd_soc_dai_link tiny210_dai = {
    .name = "TINY210-IIS",
    .stream_name = "WM8960 HiFi",
    .codec_name = "wm8960-codec.0-001a",
    .platform_name = "samsung-audio",
    .cpu_dai_name = "samsung-i2s.0",
    .codec_dai_name = "wm8960-hifi",
    .init = tiny210_wm8960_init,
    .ops = &tiny210_wm8960_ops,
};

static struct snd_soc_card tiny210_soc_card = {
    .name = "tiny210",
    .dai_link = &tiny210_dai,
    .num_links = 1,
};

static struct platform_device *tiny210_snd_device;

static int __init tiny210_audio_init(void)
{
    int ret;

    tiny210_snd_device = platform_device_alloc("soc-audio", -1);
    if ( !tiny210_snd_device ){
        return -ENOMEM;
    }

    platform_set_drvdata( tiny210_snd_device, &tiny210_soc_card );

    ret = platform_device_add( tiny210_snd_device );
    if( ret ){
        platform_device_put( tiny210_snd_device );
    }

    return ret;
}

static void __exit tiny210_audio_exit(void)
{
    platform_device_unregister( tiny210_snd_device );
}


module_init( tiny210_audio_init );
module_exit( tiny210_audio_exit );

MODULE_AUTHOR("hbjsxieqi@163.com");
MODULE_DESCRIPTION("ALSA SoC TINY210 WM8960");
MODULE_LICENSE("GPL");

