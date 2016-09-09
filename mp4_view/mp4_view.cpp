// mp4_view.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "stdio.h"
#include "string.h"
#include "stdarg.h"
#include "time.h"

static bool verbos = false;
static int indent = 0;
static unsigned __int64 file_size = 0;

struct keep_indent
{
	keep_indent() {indent++;}
	~keep_indent() {indent--;}
};

void my_printf(const char* format, ...)
{
	for (int i = 0; i < indent; i++)
	{
		printf("\t");
	}

	va_list args;
	va_start (args, format);
	vprintf (format, args);
	va_end (args);
}

void my_wprintf(const wchar_t* format, ...)
{
	for (int i = 0; i < indent; i++)
	{
		printf("\t");
	}

	va_list args;
	va_start (args, format);
	vwprintf (format, args);
	va_end (args);
}

// net order to little endian
template <class T>
T ntole(T num)
{
	T ret = 0;
	unsigned char * p = (unsigned char*)&num;
	for(int i = 0; i < sizeof(T); i++)
		ret |= (p[i] << ((sizeof(T) - i - 1) * 8));

	return ret;
}

enum HDLRTYPE
{
	VIDEO = 0,
	SOUND,
	HINT
};

enum BOXTYPE
{
	FTYP = 0,
	MOOV,
	IODS,
	MVHD,
	TRAK,
	TKHD,
	MDIA,
	MDHD,
	HDLR,
	MINF,
	VMHD,
	SMHD,
	DINF,
	STBL,
	STSD,
	STTS,
	CTTS,
	STSC,
	STSZ,
	STZ2,
	STCO,
	CO64,
	STSS,
	URL0,
	URN0,
	DREF,
	UUID,
	AVCC,
	BTRT,

	SKIP
};

unsigned long box_type(char* type)
{
	if (type == NULL)
		return 0;

	unsigned long ret = 0;
	for (int i = 0; i < 4; i++)
		ret |= type[i] << ((4-i-1)*8);
	return ret;
}

BOXTYPE detect_type(FILE* fmp4)
{
	int ret = fseek(fmp4, 4, SEEK_CUR);
	unsigned long type = 0;
	ret = fread(&type, 4, 1, fmp4);
	if (ret == 0)
		return SKIP;

	type = ntole(type);
	fseek(fmp4, -8, SEEK_CUR);

	static unsigned long type_repo[] = {box_type("ftyp"),
		box_type("moov"),
		box_type("iods"),
		box_type("mvhd"),
		box_type("trak"),
		box_type("tkhd"),
		box_type("mdia"),
		box_type("mdhd"),
		box_type("hdlr"),
		box_type("minf"),
		box_type("vmhd"),
		box_type("smhd"),
		box_type("dinf"),
		box_type("stbl"),
		box_type("stsd"),
		box_type("stts"),
		box_type("ctts"),
		box_type("stsc"),
		box_type("stsz"),
		box_type("stz2"),
		box_type("stco"),
		box_type("co64"),
		box_type("stss"),
		box_type("url "),
		box_type("urn "),
		box_type("dref"),
		box_type("uuid"),
		box_type("avcC"),
		box_type("btrt"),
	};

	for (int i = 0; i < SKIP; i++)
	{
		if (type_repo[i] == type)
			return (BOXTYPE)i;
	}

	return SKIP;
}

unsigned long convert_utf8_to_widechar(char* str, wchar_t* wcs, unsigned long len)
{
	unsigned long i = 0;		// utf-8 string pos
	unsigned long cw = 1;		// utf-8 char length
	unsigned long w = 0;		// wchar_t pos;
	while ( i < len )
	{
		wcs[w] = 0;
		if (str[i] == 0) 
			break;

		if (str[i] < 0x80) { cw = 1; wcs[w] = str[i]; }
		else if (str[i] < 0xE0) { cw = 2; wcs[w] |= (0x1F & str[i]) << 3; }
		else if (str[i] < 0xF0) { cw = 3; wcs[w] |= (0x0F & str[i]) << 4; }
//		else if (str[i] < 0xF8) cw = 4;
//		else if (str[i] < 0xFC) cw = 5;
//		else if (str[i] < 0xFE) cw = 6;

		for (unsigned long j = 1; j < cw; j++)
		{
			wcs[w] |= (0x3F & str[i+j]) << 2;
		}
		if (cw == 2)
			wcs[w] = wcs[w] >> 4;

		i += cw;
		w++;
	}

	return i+1;
}

void form_uuid(unsigned char * uuid, char* uuid_str)
{
	_snprintf_s(uuid_str, 37, 36, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
		uuid[0],uuid[1],uuid[2],uuid[3],uuid[4],uuid[5],uuid[6],uuid[7],
		uuid[8],uuid[9],uuid[10],uuid[11],uuid[12],uuid[13],uuid[14],uuid[15]);
}

unsigned __int64 DispMP4Box(FILE* fmp4, unsigned long * pos)
{
	unsigned long  size = 0;
	unsigned __int64 llsize = 0;
	char boxType[5] = {0};
	int ret = fread(&size, 4, 1, fmp4);
	if (ret == 0)
		return 0;
	ret = fread(&boxType, 1, 4, fmp4);

	*pos = 8;

	size = ntole(size);

	llsize = size;
	if (size == 1) {
		ret = fread(&llsize, 8, 1, fmp4);
		llsize = ntole(llsize);

		*pos += 8;
	}
	else if (size == 0)
	{
		unsigned __int64 pos = _ftelli64(fmp4);
		llsize = file_size - pos;
	}

	printf("\n");
	my_printf("%s:%llu bytes  ", boxType, llsize);

	if (strcmp(boxType, "uuid") == 0) {
		unsigned char user_type[17] = {0};
		char user_uuid[37] = {0};
		fread(&user_type, 1, 16, fmp4);

		form_uuid(user_type, user_uuid);
		printf("user type: %s", user_uuid);

		*pos += 16;
	}
	printf("\n");

	return llsize;
}

unsigned __int64 DispMP4FullBox(FILE* fmp4, unsigned char *ver, unsigned long *flags, unsigned long * box_pos)
{
	unsigned __int64 size = DispMP4Box(fmp4, box_pos);
	if (size == 0)
		return 0;

	*box_pos += 4;

	int ret = fread(flags, 4, 1, fmp4);
	*flags = ntole(*flags);

	*ver = *flags >> 24;
	*flags &= 0xFFF;

	my_printf("ver:0x%02x flags:0x%06x\n", *ver, *flags);

	return size;
}

unsigned __int64 DispFtyp(FILE* fmp4)
{
	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4Box(fmp4, &box_pos);

	unsigned char major_brand[5] = {0};
	unsigned long minor_version = 0;
	fread(major_brand, 4, 1, fmp4);
	fread(&minor_version, 4, 1, fmp4);

	my_printf("\tmajor_brand:%s minor_version:0x%08X", major_brand, minor_version);

	int loop = (int)(size - box_pos - 8) / 4;
	printf("\tCompatible Brands:");
	for (int i = 0; i < loop; i++)
	{
		unsigned char brands[5] = {0};
		fread(brands, 4, 1, fmp4);
		my_printf(" %s", brands);
	}
	printf("\n");

	return size;
}

unsigned __int64 DispOther(FILE* fmp4)
{
	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4Box(fmp4, &box_pos);
	if (size == 0)
		return 0;
	_fseeki64(fmp4, (size - box_pos), SEEK_CUR);
	return size;
}

void set_time_str(unsigned __int64 time, char* time_str)
{
	tm rtime;
	gmtime_s(&rtime, (time_t*)&time);

	_snprintf_s(time_str, 20, 19, "%04u/%02u/%02u %02u:%02u:%02u", rtime.tm_year+1900-66, rtime.tm_mon+1, rtime.tm_mday, 
		rtime.tm_hour, rtime.tm_min, rtime.tm_sec);
}

unsigned __int64 DispMVHD(FILE* fmp4, unsigned __int64 *time_scale)
{
	keep_indent ki;

	unsigned char ver = 0;
	unsigned long flags = 0;
	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4FullBox(fmp4, &ver, &flags, &box_pos);

	int ret = 0;
	char c_time_str[20] = {0};
	char m_time_str[20] = {0};
	if (ver == 1)
	{
		unsigned __int64 creation_time; 
		unsigned __int64 modification_time; 
		unsigned long timescale; 
		unsigned __int64 duration; 
		ret = fread(&creation_time, 8, 1, fmp4);
		creation_time = ntole(creation_time);
		ret = fread(&modification_time, 8, 1, fmp4);
		modification_time = ntole(modification_time);
		ret = fread(&timescale, 4, 1, fmp4);
		timescale = ntole(timescale);
		ret = fread(&duration, 8, 1, fmp4);
		duration = ntole(duration);

		*time_scale = timescale;

		set_time_str(creation_time, c_time_str);
		set_time_str(modification_time, m_time_str);

		my_printf("\tcreation_time:%s,modification_time:%s,timescale:%lu/s,duration:%llu s\n",
			c_time_str,m_time_str,timescale,duration/timescale);
	}
	else {
		unsigned long creation_time; 
		unsigned long modification_time; 
		unsigned long timescale; 
		unsigned long duration; 
		ret = fread(&creation_time, 4, 1, fmp4);
		creation_time = ntole(creation_time);
		ret = fread(&modification_time, 4, 1, fmp4);
		modification_time = ntole(modification_time);
		ret = fread(&timescale, 4, 1, fmp4);
		timescale = ntole(timescale);
		ret = fread(&duration, 4, 1, fmp4);
		duration = ntole(duration);

		*time_scale = timescale;

		set_time_str(creation_time, c_time_str);
		set_time_str(modification_time, m_time_str);

		my_printf("\tcreation_time:%s,modification_time:%s,timescale:%lu/s,duration:%lu s\n",
			c_time_str,m_time_str,timescale,duration/timescale);
	}

	unsigned long rate = 0x00010000;
	ret = fread(&rate, 4, 1, fmp4);
	rate = ntole(rate);
	unsigned short volume = 0x0100;  // typically, full volume 
	ret = fread(&volume, 2, 1, fmp4);
	volume = ntole(volume);

#if 0
	const bit(16)  reserved = 0; 
	const unsigned int(32)[2]  reserved = 0; 
	template int(32)[9]  matrix = 
	{ 0x00010000,0,0,0,0x00010000,0,0,0,0x40000000 }; 
	// Unity matrix 
	bit(32)[6]  pre_defined = 0; 
#else
	fseek(fmp4, 70, SEEK_CUR);
#endif
	unsigned long next_track_ID;
	ret = fread(&next_track_ID, 4, 1, fmp4);
	next_track_ID = ntole(next_track_ID);

	my_printf("\trate:0x%08x,volume:0x%04x,next_track_ID:%lu\n", rate, volume, next_track_ID);

	return size;
}

unsigned __int64 DispTKHD(FILE* fmp4, unsigned __int64 time_scale)
{
	keep_indent ki;

	unsigned char ver = 0;
	unsigned long flags = 0;
	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4FullBox(fmp4, &ver, &flags, &box_pos);

	int ret = 0;
	char c_time_str[20] = {0};
	char m_time_str[20] = {0};
	if (ver == 1)
	{
		unsigned __int64 creation_time; 
		unsigned __int64 modification_time; 
		unsigned long track_ID; 
		unsigned __int64 duration; 
		ret = fread(&creation_time, 8, 1, fmp4);
		creation_time = ntole(creation_time);
		ret = fread(&modification_time, 8, 1, fmp4);
		modification_time = ntole(modification_time);
		ret = fread(&track_ID, 4, 1, fmp4);
		track_ID = ntole(track_ID);
		fseek(fmp4, 4, SEEK_CUR);
		ret = fread(&duration, 8, 1, fmp4);
		duration = ntole(duration);

		set_time_str(creation_time, c_time_str);
		set_time_str(modification_time, m_time_str);

		if (duration == 0xFFFFFFFFFFFFFFFF)
			duration = 0;
		my_printf("\tcreation_time:%s,modification_time:%s,track_ID:%lu,duration:%llu s\n",
			c_time_str,m_time_str,track_ID,duration/time_scale);
	}
	else {
		unsigned long creation_time; 
		unsigned long modification_time; 
		unsigned long track_ID; 
		unsigned long duration; 
		ret = fread(&creation_time, 4, 1, fmp4);
		creation_time = ntole(creation_time);
		ret = fread(&modification_time, 4, 1, fmp4);
		modification_time = ntole(modification_time);
		ret = fread(&track_ID, 4, 1, fmp4);
		track_ID = ntole(track_ID);
		fseek(fmp4, 4, SEEK_CUR);
		ret = fread(&duration, 4, 1, fmp4);
		duration = ntole(duration);

		set_time_str(creation_time, c_time_str);
		set_time_str(modification_time, m_time_str);

		if (duration == 0xFFFFFFFF)
			duration = 0;
		my_printf("\tcreation_time:%s,modification_time:%s,track_ID:%lu,duration:%lu s\n",
			c_time_str,m_time_str,track_ID,duration/time_scale);
	}

	// const unsigned int(32)[2]  reserved = 0;
	fseek(fmp4, 8, SEEK_CUR);

#if 0
	template int(16) layer = 0; 
	template int(16) alternate_group = 0; 
	template int(16)  volume = {if track_is_audio 0x0100 else 0}; 
	const unsigned int(16)  reserved = 0; 
	template int(32)[9]  matrix= 
	{ 0x00010000,0,0,0,0x00010000,0,0,0,0x40000000 }; 
	// unity matrix 
	unsigned int(32) width; 
	unsigned int(32) height;
#else
	short layer = 0;
	ret = fread(&layer, 2, 1, fmp4);
	layer = ntole(layer);
	short group = 0;
	ret = fread(&group, 2, 1, fmp4);
	group = ntole(group);
	short volume = 0x0100;
	ret = fread(&volume, 2, 1, fmp4);
	volume = ntole(volume);
	fseek(fmp4, 38, SEEK_CUR);
	unsigned long width = 0;
	unsigned long height = 0;
	ret = fread(&width, 4, 1, fmp4);
	width = ntole(width);
	ret = fread(&height, 4, 1, fmp4);
	height = ntole(height);

	my_printf("\tlayer:%d,group:%d,volume:0x%04x,width:0x%08x(%u),height:0x%08x(%u)\n",
		layer,group,volume,width,width>>16,height,height>>16);
#endif

	return size;
}

unsigned __int64 DispMDHD(FILE* fmp4)
{
	keep_indent ki;

	unsigned char ver = 0;
	unsigned long flags = 0;
	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4FullBox(fmp4, &ver, &flags, &box_pos);

	int ret = 0;
	char c_time_str[20] = {0};
	char m_time_str[20] = {0};
	if (ver == 1)
	{
		unsigned __int64 creation_time; 
		unsigned __int64 modification_time; 
		unsigned long timescale; 
		unsigned __int64 duration; 
		ret = fread(&creation_time, 8, 1, fmp4);
		creation_time = ntole(creation_time);
		ret = fread(&modification_time, 8, 1, fmp4);
		modification_time = ntole(modification_time);
		ret = fread(&timescale, 4, 1, fmp4);
		timescale = ntole(timescale);
		ret = fread(&duration, 8, 1, fmp4);
		duration = ntole(duration);

		set_time_str(creation_time, c_time_str);
		set_time_str(modification_time, m_time_str);

		my_printf("\tcreation_time:%s,modification_time:%s,timescale:%lu/s,duration:%llu s\n",
			c_time_str,m_time_str,timescale,duration/timescale);
	}
	else {
		unsigned long creation_time; 
		unsigned long modification_time; 
		unsigned long timescale; 
		unsigned long duration; 
		ret = fread(&creation_time, 4, 1, fmp4);
		creation_time = ntole(creation_time);
		ret = fread(&modification_time, 4, 1, fmp4);
		modification_time = ntole(modification_time);
		ret = fread(&timescale, 4, 1, fmp4);
		timescale = ntole(timescale);
		ret = fread(&duration, 4, 1, fmp4);
		duration = ntole(duration);

		set_time_str(creation_time, c_time_str);
		set_time_str(modification_time, m_time_str);

		my_printf("\tcreation_time:%s,modification_time:%s,timescale:%lu/s,duration:%lu s\n",
			c_time_str,m_time_str,timescale,duration/timescale);
	}

	unsigned short lang = 0;
	ret = fread(&lang, 2, 1, fmp4);
	lang = ntole(lang);
	my_printf("\tlanguage:%c%c%c\n", (lang>>10)+0x60, ((lang&0x3e0)>>5)+0x60, (lang&0x1f)+0x60);

	fseek(fmp4, 2, SEEK_CUR);

	return size;
}

unsigned __int64 DispHDLR(FILE* fmp4, HDLRTYPE* hdlr)
{
	keep_indent ki;

	unsigned char ver = 0;
	unsigned long flags = 0;
	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4FullBox(fmp4, &ver, &flags, &box_pos);

	int ret = 0;

	fseek(fmp4, 4, SEEK_CUR);
	char type[5] = {0};
	ret = fread(type, 1, 4, fmp4);

	if (strcmp(type, "vide") == 0)
		*hdlr = VIDEO;
	else if (strcmp(type, "soun") == 0)
		*hdlr = SOUND;
	else if (strcmp(type, "hint") == 0)
		*hdlr = HINT;

	fseek(fmp4, 12, SEEK_CUR);
	unsigned long len = (unsigned long)size - box_pos - 8 - 12;
	char* name = new char[len];
	wchar_t * wname = new wchar_t[len];
	ret = fread(name, 1, len, fmp4);
	convert_utf8_to_widechar(name, wname, len);

	my_printf("\ttype:%s,", type);
	wprintf(L"name:%s\n", wname);

	delete [] name;
	delete [] wname;

	return size;
}

bool NoBox(FILE* fmp4, unsigned long len)
{
	if (len < 8)
		return true;

	unsigned long size = 0;
	fread(&size, 4, 1, fmp4);
	size = ntole(size);

	unsigned char data[4];
	fread(data, 1, 4, fmp4);
	fseek(fmp4, -8, SEEK_CUR);

	if (size > len)
		return true;

	for (int i = 0; i < 4; i++)
	{
		if ((data[i] < 0x20) || (data[i] > 0x7E))
			return true;
	}

	return false;
}

unsigned __int64 DispAVCC(FILE* fmp4)
{
	keep_indent ki;

	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4Box(fmp4, &box_pos);

	unsigned char configurationVersion = 1; 
	fread(&configurationVersion, 1, 1, fmp4);

	unsigned char AVCProfileIndication; 
	fread(&AVCProfileIndication, 1, 1, fmp4);
	unsigned char profile_compatibility; 
	fread(&profile_compatibility, 1, 1, fmp4);
	unsigned char AVCLevelIndication;  
	fread(&AVCLevelIndication, 1, 1, fmp4);

//	bit(6) reserved = 乪111111乫b;
	unsigned char lengthSizeMinusOne;  
	fread(&lengthSizeMinusOne, 1, 1, fmp4);
	lengthSizeMinusOne &= 0x3;

//	bit(3) reserved = 乪111乫b;
	unsigned char numOfSequenceParameterSets; 
	fread(&numOfSequenceParameterSets, 1, 1, fmp4);
	numOfSequenceParameterSets &= 0x1F;

	my_printf("\tconfigurationVersion:%u,AVCProfileIndication:0x%02X,profile_compatibility:0x%02X,AVCLevelIndication:0x%02X,lengthSizeMinusOne:%u\n", 
		configurationVersion, AVCProfileIndication, profile_compatibility, AVCLevelIndication, lengthSizeMinusOne);

	my_printf("\tnumOfSequenceParameterSets:%u\n", numOfSequenceParameterSets);

	unsigned int i = 0;
	for (i=0; i< numOfSequenceParameterSets;  i++) { 
		unsigned short sequenceParameterSetLength ; 
		fread(&sequenceParameterSetLength, 2, 1, fmp4);
		sequenceParameterSetLength = ntole(sequenceParameterSetLength);

//		bit(8*sequenceParameterSetLength) sequenceParameterSetNALUnit; 
		fseek(fmp4, sequenceParameterSetLength, SEEK_CUR);
	} 

	unsigned char numOfPictureParameterSets; 
	fread(&numOfPictureParameterSets, 1, 1, fmp4);
	my_printf("\tnumOfPictureParameterSets:%u\n", numOfPictureParameterSets);

	for (i=0; i< numOfPictureParameterSets;  i++) { 
		unsigned short pictureParameterSetLength; 
		fread(&pictureParameterSetLength, 2, 1, fmp4);
		pictureParameterSetLength = ntole(pictureParameterSetLength);

//		bit(8*pictureParameterSetLength) pictureParameterSetNALUnit; 
		fseek(fmp4, pictureParameterSetLength, SEEK_CUR);
	} 

	return size;
}

unsigned __int64 DispBTRT(FILE* fmp4)
{
	keep_indent ki;

	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4Box(fmp4, &box_pos);

	unsigned long bufferSizeDB; 
	unsigned long maxBitrate; 
	unsigned long avgBitrate;

	fread(&bufferSizeDB, 4, 1, fmp4);
	fread(&maxBitrate, 4, 1, fmp4);
	fread(&avgBitrate, 4, 1, fmp4);

	bufferSizeDB = ntole(bufferSizeDB);
	maxBitrate = ntole(maxBitrate);
	avgBitrate = ntole(avgBitrate);

	my_printf("\tbufferSizeDB:%u bytes, maxBitrate:%u bps, avgBitrate:%u bps\n", bufferSizeDB, maxBitrate, avgBitrate);

	return size;
}

void DispSampleDesc(FILE* fmp4, HDLRTYPE hdlr, unsigned long size)
{
	keep_indent ki;

	switch (hdlr)
	{
	case VIDEO:
		{
			unsigned long box_pos = 0;
			unsigned __int64 size = DispMP4Box(fmp4, &box_pos);
			unsigned __int64 pos  = 0;

			fseek(fmp4, 6, SEEK_CUR);
			unsigned short ref = 0;
			fread(&ref, 2, 1, fmp4);
			ref = ntole(ref);

			my_printf("\tdata_reference_index:%d\n", ref);

			fseek(fmp4, 16, SEEK_CUR);

			unsigned short width = 0;
			unsigned short height = 0;
			fread(&width, 2, 1, fmp4);
			width = ntole(width);
			fread(&height, 2, 1, fmp4);
			height = ntole(height);

			unsigned long hres = 0x00480000;
			unsigned long vres = 0x00480000;
			fread(&hres, 4, 1, fmp4);
			hres = ntole(hres);
			fread(&vres, 4, 1, fmp4);
			vres = ntole(vres);

			fseek(fmp4, 4, SEEK_CUR);

			unsigned short frame_count = 1;
			fread(&frame_count, 2, 1, fmp4);
			frame_count = ntole(frame_count);

			unsigned char codec_name[32] = {0};
			unsigned char name_len = 0;
			fread(&name_len, 1, 1, fmp4);
			if (name_len > 0)
				fread(codec_name, 1, name_len, fmp4);
			if (name_len < 31)
				fseek(fmp4, 31-name_len, SEEK_CUR);

			unsigned short depth = 0x18;
			fread(&depth, 2, 1, fmp4);
			depth = ntole(depth);

			fseek(fmp4, 2, SEEK_CUR);

			my_printf("\twidth:%d,height:%d,hres:%08x(%d),vres:%08x(%d),frame_count:%d,compressorName:%s,depth:%d\n",
				width, height, hres, hres>>16, vres, vres>>16, frame_count, codec_name, depth);

			pos += 78;	// not include box_pos

			while (pos < size - box_pos) 
			{
				unsigned long room = (unsigned long)(size - pos - box_pos);
				if (NoBox(fmp4, room)) {
					fseek(fmp4, room, SEEK_CUR);
					break;
				}

				BOXTYPE type = detect_type(fmp4);
				if (type == AVCC)
					pos += DispAVCC(fmp4);
				else if (type == BTRT)
					pos += DispBTRT(fmp4);
				else {
					keep_indent ki;
					pos += DispOther(fmp4);
				}
			} 
		}
		break;
	case SOUND:
		{
			unsigned long box_pos = 0;
			unsigned __int64 size = DispMP4Box(fmp4, &box_pos);
			unsigned __int64 pos  = 0;

			fseek(fmp4, 6, SEEK_CUR);
			unsigned short ref = 0;
			fread(&ref, 2, 1, fmp4);
			ref = ntole(ref);

			my_printf("\tdata_reference_index:%d\n", ref);

			fseek(fmp4, 8, SEEK_CUR);

			unsigned short channel_count = 2;
			unsigned short sample_size = 16;
			fread(&channel_count, 2, 1, fmp4);
			channel_count = ntole(channel_count);
			fread(&sample_size, 2, 1, fmp4);
			sample_size = ntole(sample_size);

			fseek(fmp4, 4, SEEK_CUR);

			unsigned long sample_rate = 0;
			fread(&sample_rate, 4, 1, fmp4);
			sample_rate = ntole(sample_rate);

			my_printf("\tchannel_count:%d,sample_size:%d,sample_rate:0x%08x(%d)\n", 
				channel_count, sample_size, sample_rate, sample_rate>>16);

			pos += 28;

			while (pos < size - box_pos)
			{
				unsigned long room = (unsigned long)(size - pos - box_pos);
				if (NoBox(fmp4, room)) {
					fseek(fmp4, room, SEEK_CUR);
					break;
				}

				keep_indent ki;
				pos += DispOther(fmp4);
			}
		}
		break;
	case HINT:
		fseek(fmp4, size, SEEK_CUR);
		break;
	}
}

static char* HDLR_TYPE_STRING [] = {"Video", "Sound", "Hint"};
unsigned __int64 DispSTSD(FILE* fmp4, HDLRTYPE hdlr)
{
	keep_indent ki;

	unsigned char ver = 0;
	unsigned long flags = 0;
	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4FullBox(fmp4, &ver, &flags, &box_pos);

	int ret = 0;
	unsigned long entry_count = 0;
	ret = fread(&entry_count, 4, 1, fmp4);
	entry_count = ntole(entry_count);
	my_printf("\tentry_count:%d (%s)\n",entry_count, HDLR_TYPE_STRING[hdlr]);

	unsigned long entry_size = ((unsigned long)size - 16) / entry_count;
	for (unsigned int i = 0; i < entry_count; i++)
	{
		DispSampleDesc(fmp4, hdlr, entry_size);
	}

	return size;
}

unsigned __int64 DispSTTS(FILE* fmp4)
{
	keep_indent ki;

	unsigned char ver = 0;
	unsigned long flags = 0;
	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4FullBox(fmp4, &ver, &flags, &box_pos);

	int ret = 0;
	unsigned long entry_count = 0;
	ret = fread(&entry_count, 4, 1, fmp4);
	entry_count = ntole(entry_count);
	my_printf("\tentry_count:%d (sample_count, sample_delta)\n",entry_count);

	for (unsigned int i = 0; i < entry_count; i++)
	{
		unsigned long sample_count = 0;
		unsigned long sample_delta = 0;
		ret = fread(&sample_count, 4, 1, fmp4);
		sample_count = ntole(sample_count);
		ret = fread(&sample_delta, 4, 1, fmp4);
		sample_delta = ntole(sample_delta);

		if (verbos)
			my_printf("\t\t%d: %d, %d\n", i, sample_count, sample_delta);
	}

	return size;
}

unsigned __int64 DispCTTS(FILE* fmp4)
{
	keep_indent ki;

	unsigned char ver = 0;
	unsigned long flags = 0;
	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4FullBox(fmp4, &ver, &flags, &box_pos);

	int ret = 0;
	unsigned long entry_count = 0;
	ret = fread(&entry_count, 4, 1, fmp4);
	entry_count = ntole(entry_count);
	my_printf("\tentry_count:%d (sample_count, sample_offset)\n",entry_count);

	for (unsigned int i = 0; i < entry_count; i++)
	{
		unsigned long sample_count = 0;
		unsigned long sample_offset = 0;
		ret = fread(&sample_count, 4, 1, fmp4);
		sample_count = ntole(sample_count);
		ret = fread(&sample_offset, 4, 1, fmp4);
		sample_offset = ntole(sample_offset);

		if (verbos)
			my_printf("\t\t%d: %d, %d\n", i, sample_count, sample_offset);
	}

	return size;
}

unsigned __int64 DispSTSC(FILE* fmp4)
{
	keep_indent ki;

	unsigned char ver = 0;
	unsigned long flags = 0;
	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4FullBox(fmp4, &ver, &flags, &box_pos);

	int ret = 0;
	unsigned long entry_count = 0;
	ret = fread(&entry_count, 4, 1, fmp4);
	entry_count = ntole(entry_count);
	my_printf("\tentry_count:%d (first_chunk, samples_per_chunk, sample_desc_index)\n",entry_count);

	for (unsigned int i = 0; i < entry_count; i++)
	{
		unsigned long first_chunk = 0;
		unsigned long sample_per_chunk = 0;
		unsigned long sample_desc_index = 0;

		ret = fread(&first_chunk, 4, 1, fmp4);
		first_chunk = ntole(first_chunk);
		ret = fread(&sample_per_chunk, 4, 1, fmp4);
		sample_per_chunk = ntole(sample_per_chunk);
		ret = fread(&sample_desc_index, 4, 1, fmp4);
		sample_desc_index = ntole(sample_desc_index);

		if (verbos)
			my_printf("\t\t%d: %u, %u, %u\n", i, first_chunk, sample_per_chunk, sample_desc_index);
	}

	return size;
}

unsigned __int64 DispSTSZ(FILE* fmp4)
{
	keep_indent ki;

	unsigned char ver = 0;
	unsigned long flags = 0;
	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4FullBox(fmp4, &ver, &flags, &box_pos);

	int ret = 0;

	unsigned long sample_size = 0;
	unsigned long sample_count = 0;

	ret = fread(&sample_size, 4, 1, fmp4);
	sample_size = ntole(sample_size);
	ret = fread(&sample_count, 4, 1, fmp4);
	sample_count = ntole(sample_count);

	my_printf("\tsample_size:%d, sample_count:%d (entry_size)\n", sample_size, sample_count);

	if (sample_size == 0) {
		for (unsigned int i = 0; i < sample_count; i++)
		{
			unsigned long entry_size = 0;
			ret = fread(&entry_size, 4, 1, fmp4);
			entry_size = ntole(entry_size);

			if (verbos)
				my_printf("\t\t%d: %lu\n", i, entry_size);
		}
	}

	return size;
}

unsigned __int64 DispSTZ2(FILE* fmp4)
{
	keep_indent ki;

	return DispOther(fmp4);
}

unsigned __int64 DispSTCO(FILE* fmp4)
{
	keep_indent ki;

	unsigned char ver = 0;
	unsigned long flags = 0;
	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4FullBox(fmp4, &ver, &flags, &box_pos);

	int ret = 0;
	unsigned long entry_count = 0;
	ret = fread(&entry_count, 4, 1, fmp4);
	entry_count = ntole(entry_count);
	my_printf("\tentry_count:%d (chunk_offset)\n",entry_count);

	for (unsigned int i = 0; i < entry_count; i++)
	{
		unsigned long chunk_offset = 0;
		ret = fread(&chunk_offset, 4, 1, fmp4);
		chunk_offset = ntole(chunk_offset);

		if (verbos)
			my_printf("\t\t%d: %lu\n", i, chunk_offset);
	}

	return size;
}

unsigned __int64 DispCO64(FILE* fmp4)
{
	keep_indent ki;

	unsigned char ver = 0;
	unsigned long flags = 0;
	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4FullBox(fmp4, &ver, &flags, &box_pos);

	int ret = 0;
	unsigned long entry_count = 0;
	ret = fread(&entry_count, 4, 1, fmp4);
	entry_count = ntole(entry_count);
	my_printf("\tentry_count:%d (chunk_offset)\n",entry_count);

	for (unsigned int i = 0; i < entry_count; i++)
	{
		unsigned __int64 chunk_offset = 0;
		ret = fread(&chunk_offset, 8, 1, fmp4);
		chunk_offset = ntole(chunk_offset);

		if (verbos)
			my_printf("\t\t%d: %llu\n", i, chunk_offset);
	}

	return size;
}

unsigned __int64 DispSTSS(FILE* fmp4)
{
	keep_indent ki;

	unsigned char ver = 0;
	unsigned long flags = 0;
	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4FullBox(fmp4, &ver, &flags, &box_pos);

	int ret = 0;
	unsigned long entry_count = 0;
	ret = fread(&entry_count, 4, 1, fmp4);
	entry_count = ntole(entry_count);
	my_printf("\tentry_count:%d (sample_number)\n",entry_count);

	for (unsigned int i = 0; i < entry_count; i++)
	{
		unsigned long sample_number = 0;
		ret = fread(&sample_number, 4, 1, fmp4);
		sample_number = ntole(sample_number);

		if (verbos)
			my_printf("\t\t%d: %u\n", i, sample_number);
	}

	return size;
}

unsigned __int64 DispSTBL(FILE* fmp4, HDLRTYPE hdlr)
{
	keep_indent ki;

	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4Box(fmp4, &box_pos);
	unsigned __int64 pos = 0;

	do 
	{
		BOXTYPE type = detect_type(fmp4);
		if (type == STSD)
			pos += DispSTSD(fmp4, hdlr);
		else if (type == STTS)
			pos += DispSTTS(fmp4);
		else if (type == CTTS)
			pos += DispCTTS(fmp4);
		else if (type == STSC)
			pos += DispSTSC(fmp4);
		else if (type == STSZ)
			pos += DispSTSZ(fmp4);
		else if (type == STZ2)
			pos += DispSTZ2(fmp4);
		else if (type == STCO)
			pos += DispSTCO(fmp4);
		else if (type == CO64)
			pos += DispCO64(fmp4);
		else if (type == STSS)
			pos += DispSTSS(fmp4);
		else
		{
			keep_indent ki;
			pos += DispOther(fmp4);
		}
	} while (pos < size - box_pos);

	return size;
}

unsigned __int64 DispVMHD(FILE* fmp4)
{
	keep_indent ki;

	unsigned char ver = 0;
	unsigned long flags = 0;
	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4FullBox(fmp4, &ver, &flags, &box_pos);

	int ret = 0;
	unsigned short graphics_mode = 0;
	unsigned short op_color[3] = {0};
	ret = fread(&graphics_mode, 2, 1, fmp4);
	graphics_mode = ntole(graphics_mode);
	for (int i = 0; i < 3; i++) {
		ret = fread(&op_color[i], 2, 1, fmp4);
		op_color[i] = ntole(op_color[i]);
	}

	my_printf("\tgraphics_mode:%d, op_color(RGB):{%d,%d,%d}\n", graphics_mode, op_color[0],op_color[1],op_color[2]);

	return size;
}

unsigned __int64 DispSMHD(FILE* fmp4)
{
	keep_indent ki;

	unsigned char ver = 0;
	unsigned long flags = 0;
	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4FullBox(fmp4, &ver, &flags, &box_pos);

	int ret = 0;
	unsigned short balance = 0;
	ret = fread(&balance, 2, 1, fmp4);
	balance = ntole(balance);
	fseek(fmp4, 2, SEEK_CUR);

	my_printf("\tBalance:%d\n", balance);

	return size;
}

unsigned __int64 DispURL(FILE* fmp4)
{
	keep_indent ki;

	unsigned char ver = 0;
	unsigned long flags = 0;
	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4FullBox(fmp4, &ver, &flags, &box_pos);

	if (flags == 1) {
		my_printf("\tLocation: Same file.\n");
		return size;
	}

	int ret = 0;

	unsigned long len = (unsigned long)size - box_pos;
	char * location = new char[len];
	wchar_t * wlocation = new wchar_t[len];
	ret = fread(location, 1, len, fmp4);
	convert_utf8_to_widechar(location, wlocation, len);

	my_wprintf(L"\tLocation:%s\n", wlocation);
	delete [] location;
	delete [] wlocation;

	return size;
}

unsigned __int64 DispURN(FILE* fmp4)
{
	keep_indent ki;

	unsigned char ver = 0;
	unsigned long flags = 0;
	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4FullBox(fmp4, &ver, &flags, &box_pos);

	if (flags == 1) {
		my_printf("\tLocation: Same file.\n");
		return size;
	}

	int ret = 0;

	unsigned long len = (unsigned long)size - box_pos;
	char * location = new char[len];
	wchar_t * wlocation = new wchar_t[len];
	ret = fread(location, 1, len, fmp4);

	unsigned long pos = convert_utf8_to_widechar(location, wlocation, len);
	my_wprintf(L"\tName:%s", wlocation);

	convert_utf8_to_widechar(location + pos, wlocation, len - pos);
	my_wprintf(L"\tLocation:%s\n", wlocation);

	delete [] location;
	delete [] wlocation;

	return size;
}

unsigned __int64 DispDREF(FILE* fmp4)
{
	keep_indent ki;

	unsigned char ver = 0;
	unsigned long flags = 0;
	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4FullBox(fmp4, &ver, &flags, & box_pos);

	int ret = 0;

	unsigned long entry_count = 0;
	ret = fread(&entry_count, 4, 1, fmp4);
	entry_count = ntole(entry_count);
	my_printf("\tEntry_count:%u\n", entry_count);

	for (unsigned long i = 0; i < entry_count; i++)
	{
		if (detect_type(fmp4) == URL0)
			DispURL(fmp4);
		else if (detect_type(fmp4) == URN0)
			DispURN(fmp4);
		else {
			keep_indent ki;
			DispOther(fmp4);
		}
	}

	return size;
}

unsigned __int64 DispDINF(FILE* fmp4)
{
	keep_indent ki;

	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4Box(fmp4, &box_pos);
	unsigned __int64 pos = 0;

	do 
	{
		BOXTYPE type = detect_type(fmp4);
		if (type == DREF)
			pos += DispDREF(fmp4);
		else
		{
			keep_indent ki;
			pos += DispOther(fmp4);
		}
	} while (pos < size - box_pos);

	return size;
}


unsigned __int64 DispMINF(FILE* fmp4, HDLRTYPE hdlr)
{
	keep_indent ki;

	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4Box(fmp4, &box_pos);
	unsigned __int64 pos = 0;

	do 
	{
		BOXTYPE type = detect_type(fmp4);
		if (type == VMHD)
			pos += DispVMHD(fmp4);
		else if (type == SMHD)
			pos += DispSMHD(fmp4);
		else if (type == DINF)
			pos += DispDINF(fmp4);
		else if (type == STBL)
			pos += DispSTBL(fmp4, hdlr);
		else
		{
			keep_indent ki;
			pos += DispOther(fmp4);
		}
	} while (pos < size - box_pos);

	return size;
}

unsigned __int64 DispMDIA(FILE* fmp4)
{
	keep_indent ki;

	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4Box(fmp4, &box_pos);
	unsigned __int64 pos = 0;

	HDLRTYPE hdlr = VIDEO;

	do 
	{
		BOXTYPE type = detect_type(fmp4);
		if (type == MDHD)
			pos += DispMDHD(fmp4);
		else if (type == HDLR)
			pos += DispHDLR(fmp4, &hdlr);
		else if (type == MINF)
			pos += DispMINF(fmp4, hdlr);
		else
		{
			keep_indent ki;
			pos += DispOther(fmp4);
		}
	} while (pos < size - box_pos);

	return size;
}

unsigned __int64 DispTRAK(FILE* fmp4, unsigned __int64 time_scale)
{
	keep_indent ki;

	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4Box(fmp4, &box_pos);
	unsigned __int64 pos = 0;

	do 
	{
		BOXTYPE type = detect_type(fmp4);
		if (type == TKHD)
			pos += DispTKHD(fmp4, time_scale);
		else if (type == MDIA)
			pos += DispMDIA(fmp4);
		else
		{
			keep_indent ki;
			pos += DispOther(fmp4);
		}
	} while (pos < size - box_pos);

	return size;
}

unsigned __int64 DispIODS(FILE* fmp4)
{
	keep_indent ki;

	unsigned char ver = 0;
	unsigned long flags = 0;
	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4FullBox(fmp4, &ver, &flags, &box_pos);

	int ret = 0;

	// iods OD/IOD (ObjectDescriptor/InitialObjectDescriptor part-1 8.6.2-4)
	fseek(fmp4, (unsigned long)size - box_pos, SEEK_CUR);
	my_printf("\tObjectDescriptor/InitialObjectDescriptor (part-1 8.6.2-4)\n");

	return size;
}

unsigned __int64 DispMOOV(FILE* fmp4)
{
	unsigned long box_pos = 0;
	unsigned __int64 size = DispMP4Box(fmp4, &box_pos);
	unsigned __int64 pos = 0;

	unsigned __int64 time_scale = 1;

	do 
	{
		BOXTYPE type = detect_type(fmp4);
		if (type == MVHD)
			pos += DispMVHD(fmp4, &time_scale);
		else if (type == TRAK)
			pos += DispTRAK(fmp4, time_scale);
		else if (type == IODS)
			pos += DispIODS(fmp4);
		else
		{
			keep_indent ki;
			pos += DispOther(fmp4);
		}
	} while (pos < size - box_pos);  // 8bytes = MOOV Box header

	return size;
}

int main(int argc, char* argv[])
{
	argc = 3;
	if (argc < 2) {
		my_printf("Usage: mp4_view mp4_file_name\n");
		return 0;
	}

	if (argc > 2)
		verbos = false;

	FILE* fmp4 = NULL;
	char *filename = "G:\\1.mp4";
	errno_t ret = fopen_s(&fmp4, filename, "rb");
	if (ret != 0)
		return 0;
	//文件指针移到末尾
	_fseeki64(fmp4, 0, SEEK_END);
	//得到当前文件指针位置
	file_size = _ftelli64(fmp4);
	//移到头部
	_fseeki64(fmp4, 0, SEEK_SET);

	unsigned __int64 pos = 0;

	pos += DispFtyp(fmp4);

	do 
	{
		if (detect_type(fmp4) == MOOV)
			pos += DispMOOV(fmp4);
		else
			pos += DispOther(fmp4);
	} while (pos < file_size);

	fclose(fmp4);
	getchar();
	return 0;
}

