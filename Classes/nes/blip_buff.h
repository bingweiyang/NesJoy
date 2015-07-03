//
//  blip_buff.h
//  NesEmulator
//
//  Created by tianshuai on 6/29/15.
//
//

#ifndef __NesEmulator__blip_buff__
#define __NesEmulator__blip_buff__

/*
Overview
--------
This library resamples audio waveforms from input clock rate to output
sample rate. Usage follows this general pattern:

* Create buffer with blip_new().
* Set clock rate and sample rate with blip_set_rates().
* Waveform generation loop:
- Generate several clocks of waveform with blip_add_delta().
- End time frame with blip_end_frame().
- Read samples from buffer with blip_read_samples().
* Free buffer with blip_delete().


Buffer creation
---------------
Before synthesis, a buffer must be created with blip_new(). Its size is
the maximum number of unread samples it can hold. For most uses, this
can be 1/10 the sample rate or less, since samples will usually be read
out immediately after being generated.

After the buffer is created, the input clock rate and output sample rate
must be set with blip_set_rates(). This determines how many input clocks
there are per second, and how many output samples are generated per
second.

If the compiler supports a 64-bit integer type, then the input-output
ratio is stored very accurately. If the compiler only supports a 32-bit
integer type, then the ratio is stored with only 20 fraction bits, so
some ratios cannot be represented exactly (for example, sample
                                           rate=48000 and clock rate=48001). The ratio is internally rounded up, so
there will never be fewer than 'sample rate' samples per second. Having
too many per second is generally better than having too few.


Waveform generation
-------------------
Waveforms are generated at the input clock rate. Consider a simple
square wave with 8 clocks per cycle (4 clocks high, 4 clocks low):

|<-- 8 clocks ->|
+5|        ._._._._        ._._._._        ._._._._        ._._
|        |       |       |       |       |       |       |
Amp  0|._._._._        |       |       |       |       |       |
|                |       |       |       |       |       |
-5|                ._._._._        ._._._._        ._._._._
* . . . * . . . * . . . * . . . * . . . * . . . * . . . * .
Time   0       4       8      12      16      20      24      28

The wave changes amplitude at time points 0, 4, 8, 12, 16, etc.

The following generates the amplitude at every clock of above waveform
at the input clock rate:

int wave [30];

for ( int i = 4; i < 30; ++i )
{
    if ( i % 8 < 4 )
        wave [i] = -5;
        else
            wave [i] = +5;
            }

Without this library, the wave array would then need to be resampled
from the input clock rate to the output sample rate. This library does
this resampling internally, so it won't be discussed further; waveform
generation code can focus entirely on the input clocks.

Rather than specify the amplitude at every clock, this library merely
needs to know the points where the amplitude CHANGES, referred to as a
delta. The time of a delta is specified with a clock count. The deltas
for this square wave are shown below the time points they occur at:

+5|        ._._._._        ._._._._        ._._._._        ._._
|        |       |       |       |       |       |       |
Amp  0|._._._._        |       |       |       |       |       |
|                |       |       |       |       |       |
-5|                ._._._._        ._._._._        ._._._._
* . . . * . . . * . . . * . . . * . . . * . . . * . . . * .
Time   0       4       8      12      16      20      24      28
Delta         +5     -10     +10     -10     +10     -10     +10

The following calls generate the above waveform:

blip_add_delta( blip,  4,  +5 );
blip_add_delta( blip,  8, -10 );
blip_add_delta( blip, 12, +10 );
blip_add_delta( blip, 16, -10 );
blip_add_delta( blip, 20, +10 );
blip_add_delta( blip, 24, -10 );
blip_add_delta( blip, 28, +10 );

In the examples above, the amplitudes are small for clarity. The 16-bit
sample range is -32768 to +32767, so actual waveform amplitudes would
need to be in the thousands to be audible (for example, -5000 to +5000).

This library allows waveform generation code to pay NO attention to the
output sample rate. It can focus ENTIRELY on the essence of the
waveform: the points where its amplitude changes. Since these points can
be efficiently generated in a loop, synthesis is efficient. Sound chip
emulation code can be structured to allow full accuracy down to a single
clock, with the emulated CPU being able to simply tell the sound chip to
"emulate from wherever you left off, up to clock time T within the
current time frame".


Time frames
-----------
Since time keeps increasing, if left unchecked, at some point it would
overflow the range of an integer. This library's solution to the problem
is to break waveform generation into time frames of moderate length.
Clock counts within a time frame are thus relative to the beginning of
the frame, where 0 is the beginning of the frame. When a time frame of
length T is ended, what was at time T in the old time frame is now at
time 0 in the new time frame. Breaking the above waveform into time
frames of 10 clocks each looks like this:

+5|        ._._._._        ._._._._        ._._._._        ._._
|        |       |       |       |       |       |       |
Amp  0|._._._._        |       |       |       |       |       |
|                |       |       |       |       |       |
-5|                ._._._._        ._._._._        ._._._._
* . . . * . . . * . . . * . . . * . . . * . . . * . . . * .
Time  |0       4       8  |    2       6      |0       4       8  |
| first time frame  | second time frame | third time frame  |
|<--- 10 clocks --->|<--- 10 clocks --->|<--- 10 clocks --->|

The following calls generate the above waveform. After they execute, the
first 30 clocks of the waveform will have been resampled and be
available as output samples for reading with blip_read_samples().

blip_add_delta( blip,  4,  +5 );
blip_add_delta( blip,  8, -10 );
blip_end_frame( blip, 10 );

blip_add_delta( blip,  2, +10 );
blip_add_delta( blip,  6, -10 );
blip_end_frame( blip, 10 );

blip_add_delta( blip,  0, +10 );
blip_add_delta( blip,  4, -10 );
blip_add_delta( blip,  8, +10 );
blip_end_frame( blip, 10 );
...

Time frames can be a convenient length, and the length can vary from one
frame to the next. Once a time frame is ended, the resulting output
samples become available for reading immediately, and no more deltas can
be added to it.

There is a limit of about 4000 output samples per time frame. The number
of clocks depends on the clock rate. At common sample rates, this allows
time frames of at least 1/15 second, plenty for most uses. This limit
allows increased resampling ratio accuracy.

In an emulator, it is usually convenient to have audio time frames
correspond to video frames, where the CPU's clock counter is reset at
the beginning of each video frame and thus can be used directly as the
relative clock counts for audio time frames.


Complex waveforms
-----------------
Any sort of waveform can be generated, not just a square wave. For
example, a saw-like wave:

+5|        ._._._._                ._._._._                ._._
|        |       |               |       |               |
Amp  0|._._._._        |       ._._._._        |       ._._._._
|                |       |               |       |
-5|                ._._._._                ._._._._
* . . . * . . . * . . . * . . . * . . . * . . . * . . . * .
Time   0       4       8      12      16      20      24      28
Delta         +5     -10      +5      +5     -10      +5      +5

Code to generate above waveform:

blip_add_delta( blip,  4,  +5 );
blip_add_delta( blip,  8, -10 );
blip_add_delta( blip, 12,  +5 );
blip_add_delta( blip, 16,  +5 );
blip_add_delta( blip, 20, +10 );
blip_add_delta( blip, 24,  +5 );
blip_add_delta( blip, 28,  +5 );

Similarly, multiple waveforms can be added within a time frame without
problem. It doesn't matter what order they're added, because all the
library needs are the deltas. The synthesis code doesn't need to know
all the waveforms at once either; it can calculate and add the deltas
for each waveform individually. Deltas don't need to be added in
chronological order either.


Sample buffering
----------------
Sample buffering is very flexible. Once a time frame is ended, the
resampled waveforms become output samples that are immediately made
available for reading with blip_read_samples(). They don't have to be
read immediately; they can be allowed to accumulate in the buffer, with
each time frame appending more samples to the buffer. When reading, some
or all of the samples in can be read out, with the remaining unread
samples staying in the buffer for later. Usually a program will
immediately read all available samples after ending a time frame and
play them immediately. In some systems, a program needs samples in
fixed-length blocks; in that case, it would keep generating time frames
until some number of samples are available, then read only that many,
even if slightly more were available in the buffer.

In some systems, one wants to run waveform generation for exactly the
number of clocks necessary to generate some desired number of output
samples, and no more. In that case, use blip_clocks_needed( blip, N ) to
find out how many clocks are needed to generate N additional samples.
Ending a time frame with this value will result in exactly N more
samples becoming available for reading.
*/

#ifdef __cplusplus
extern "C" {
#endif
    
    /** First parameter of most functions is blip_t*, or const blip_t* if nothing
     is changed. */
    typedef struct blip_t blip_t;
    
    /** Creates new buffer that can hold at most sample_count samples. Sets rates
     so that there are blip_max_ratio clocks per sample. Returns pointer to new
     buffer, or NULL if insufficient memory. */
    blip_t* blip_new( int sample_count );
    
    /** Sets approximate input clock rate and output sample rate. For every
     clock_rate input clocks, approximately sample_rate samples are generated. */
    void blip_set_rates( blip_t*, double clock_rate, double sample_rate );
    
    enum { /** Maximum clock_rate/sample_rate ratio. For a given sample_rate,
            clock_rate must not be greater than sample_rate*blip_max_ratio. */
        blip_max_ratio = 1 << 20 };
    
    /** Clears entire buffer. Afterwards, blip_samples_avail() == 0. */
    void blip_clear( blip_t* );
    
    /** Adds positive/negative delta into buffer at specified clock time. */
    void blip_add_delta( blip_t*, unsigned int clock_time, int delta );
    
    /** Same as blip_add_delta(), but uses faster, lower-quality synthesis. */
    void blip_add_delta_fast( blip_t*, unsigned int clock_time, int delta );
    
    /** Length of time frame, in clocks, needed to make sample_count additional
     samples available. */
    int blip_clocks_needed( const blip_t*, int sample_count );
    
    enum { /** Maximum number of samples that can be generated from one time frame. */
        blip_max_frame = 4000 };
    
    /** Makes input clocks before clock_duration available for reading as output
     samples. Also begins new time frame at clock_duration, so that clock time 0 in
     the new time frame specifies the same clock as clock_duration in the old time
     frame specified. Deltas can have been added slightly past clock_duration (up to
     however many clocks there are in two output samples). */
    void blip_end_frame( blip_t*, unsigned int clock_duration );
    
    /** Number of buffered samples available for reading. */
    int blip_samples_avail( const blip_t* );
    
    /** Reads and removes at most 'count' samples and writes them to 'out'. If
     'stereo' is true, writes output to every other element of 'out', allowing easy
     interleaving of two buffers into a stereo sample stream. Outputs 16-bit signed
     samples. Returns number of samples actually read.  */
    int blip_read_samples( blip_t*, short out [], int count, int stereo );
    
    /** Frees buffer. No effect if NULL is passed. */
    void blip_delete( blip_t* );
    
    
    /* Deprecated */
    typedef blip_t blip_buffer_t;
    
#ifdef __cplusplus
}
#endif

#endif /* defined(__NesEmulator__blip_buff__) */
