/******************************************************************************
 *
 * Project:  laszip - http://liblas.org -
 * Purpose:
 * Author:   Martin Isenburg
 *           isenburg at cs.unc.edu
 *
 ******************************************************************************
 * Copyright (c) 2009, Martin Isenburg
 *
 * This is free software; you can redistribute and/or modify it under
 * the terms of the GNU Lesser General Licence as published
 * by the Free Software Foundation.
 *
 * See the COPYING file for more information.
 *
 ****************************************************************************/

/******************************************************************************
 *
 * Project:  laszip - http://liblas.org - 
 * Purpose:  
 * Author:   Martin Isenburg
 *           martin.isenburg at gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Martin Isenburg
 * 
 * This is free software; you can redistribute and/or modify it under
 * the terms of the GNU Lesser General Licence as published
 * by the Free Software Foundation. 
 *
 * See the COPYING file for more information.
 *
 ****************************************************************************/


/*
===============================================================================

  FILE:  laspointreader2compressed.cpp
  
  CONTENTS:
  
    see corresponding header file
  
  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2008  martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    see corresponding header file
  
===============================================================================
*/

#include "laspointreader2compressed.h"

namespace laszipalternate {

bool LASpointReader2compressed::read_point(LASpoint* point, double* gps_time, unsigned short* rgb)
{
	if (dec)
	{
		*point = last_point;

    // find the median difference among the three preceding differences
    int median_x;
    if (last_x_diff[last_dir][0] < last_x_diff[last_dir][1])
    {
      if (last_x_diff[last_dir][1] < last_x_diff[last_dir][2])
        median_x = last_x_diff[last_dir][1];
      else if (last_x_diff[last_dir][0] < last_x_diff[last_dir][2])
        median_x = last_x_diff[last_dir][2];
      else
        median_x = last_x_diff[last_dir][0];
    }
    else
    {
      if (last_x_diff[last_dir][0] < last_x_diff[last_dir][2])
        median_x = last_x_diff[last_dir][0];
      else if (last_x_diff[last_dir][1] < last_x_diff[last_dir][2])
        median_x = last_x_diff[last_dir][2];
      else
        median_x = last_x_diff[last_dir][1];
    }

    int median_y;
    if (last_y_diff[last_dir][0] < last_y_diff[last_dir][1])
    {
      if (last_y_diff[last_dir][1] < last_y_diff[last_dir][2])
        median_y = last_y_diff[last_dir][1];
      else if (last_y_diff[last_dir][0] < last_y_diff[last_dir][2])
        median_y = last_y_diff[last_dir][2];
      else
        median_y = last_y_diff[last_dir][0];
    }
    else
    {
      if (last_y_diff[last_dir][0] < last_y_diff[last_dir][2])
        median_y = last_y_diff[last_dir][0];
      else if (last_y_diff[last_dir][1] < last_y_diff[last_dir][2])
        median_y = last_y_diff[last_dir][2];
      else
        median_y = last_y_diff[last_dir][1];
    }

    // decompress x y z coordinates
		int x_diff = ic_dx->DecompressLast(median_x, last_dir);
		point->x += x_diff;

    // we use the difference between the x diff prediction and the acual value to switch contexts
    float messy_measure = (float)x_diff/(float)median_x;
    int messy = (int)((messy_measure-0.5f)/0.2f);

    int y_diff;
    if (messy < 0 || messy > 6)
    {
      y_diff = ic_dy->DecompressNone();
      point->z = ic_z->DecompressNone();
    }
    else
    {
      y_diff = ic_dy->DecompressLast(median_y, (last_dir?messy+7:messy));
		  point->z = ic_z->DecompressLast(last_point.z, messy);
    }
		point->y += y_diff;

		// decompress which other values have changed
		int changed_values = dec->decode(m_changed_values);

		// decompress the intensity if it has changed
		if (changed_values & 32)
		{
			point->intensity = ic_intensity->DecompressLast(last_point.intensity);
		}

		// decompress the edge_of_flight_line, scan_direction_flag, ... if it has changed
		if (changed_values & 16)
		{
			((unsigned char*)point)[14] = dec->decode(m_bit_byte);
		}

		// decompress the classification ... if it has changed
		if (changed_values & 8)
		{
			point->classification = dec->decode(m_classification);
		}
		
		// decompress the scan_angle_rank ... if it has changed
		if (changed_values & 4)
		{
			point->scan_angle_rank = ic_scan_angle_rank->DecompressLast(last_point.scan_angle_rank, messy == 2); // this is the best messy
		}

		// decompress the user_data ... if it has changed
		if (changed_values & 2)
		{
			point->user_data = dec->decode(m_user_data);
		}

		// decompress the point_source_ID ... if it has changed
		if (changed_values & 1)
		{
			point->point_source_ID = ic_point_source_ID->DecompressLast(last_point.point_source_ID);
		}

		// decompress the rgb color ... if it has changed
		if (changed_values & 64)
		{
			rgb[0] = ic_r->DecompressLast(last_rgb[0]);
			rgb[1] = ic_g->DecompressLast(last_rgb[1]);
			rgb[2] = ic_b->DecompressLast(last_rgb[2]);
		}

    // only record the difference if the scan direction has not changed
    if (last_dir == point->scan_direction_flag)
    {
      last_x_diff[last_dir][last_incr[last_dir]] = x_diff;
      last_y_diff[last_dir][last_incr[last_dir]] = y_diff;

      last_incr[last_dir]++;
      if (last_incr[last_dir] > 2) last_incr[last_dir] = 0;
    }
  }
	else
	{
		fread(point, sizeof(LASpoint), 1, file);
		fread(rgb, sizeof(unsigned short), 3, file);
		init_decoder();
	}

  last_dir = point->scan_direction_flag;
  last_point = *point;
  last_rgb[0] = rgb[0];
  last_rgb[1] = rgb[1];
  last_rgb[2] = rgb[2];

	return true;
}

LASpointReader2compressed::LASpointReader2compressed(FILE* file)
{
	this->file = file;
  last_dir = 0;
	last_x_diff[0][0] = last_x_diff[0][1] = last_x_diff[0][2] = last_x_diff[1][0] = last_x_diff[1][1] = last_x_diff[1][2] = 0;
	last_y_diff[0][0] = last_y_diff[0][1] = last_y_diff[0][2] = last_y_diff[1][0] = last_y_diff[1][1] = last_y_diff[1][2] = 0;
  last_incr[0] = last_incr[1] = 0;
	dec = 0;
}

void LASpointReader2compressed::init_decoder()
{
	dec = new ArithmeticDecoder(file);

	ic_dx = new IntegerCompressorContext();
	ic_dy = new IntegerCompressorContext();
	ic_z = new IntegerCompressorContext();

	ic_dx->SetPrecision(32); 
	ic_dy->SetPrecision(32); 
	ic_z->SetPrecision(32); 

  ic_dx->SetupDecompressor(dec, 2);
  ic_dy->SetupDecompressor(dec, 14);
  ic_z->SetupDecompressor(dec, 7);

	m_changed_values = new ArithmeticModel(128,0,0);

	ic_intensity = new IntegerCompressorContext();
	ic_intensity->SetPrecision(16);
  ic_intensity->SetupDecompressor(dec);

	m_bit_byte = new ArithmeticModel(256,0,0);
	m_classification = new ArithmeticModel(256,0,0);

	ic_scan_angle_rank = new IntegerCompressorContext();
	ic_scan_angle_rank->SetPrecision(8);
  ic_scan_angle_rank->SetupDecompressor(dec, 2);

	m_user_data = new ArithmeticModel(256,0,0);

	ic_point_source_ID = new IntegerCompressorContext();
	ic_point_source_ID->SetPrecision(16);
  ic_point_source_ID->SetupDecompressor(dec);

	ic_r = new IntegerCompressorContext();
	ic_g = new IntegerCompressorContext();
	ic_b = new IntegerCompressorContext();

	ic_r->SetPrecision(16); 
	ic_g->SetPrecision(16); 
	ic_b->SetPrecision(16); 

	ic_r->SetupDecompressor(dec); 
	ic_g->SetupDecompressor(dec); 
	ic_b->SetupDecompressor(dec); 
}

LASpointReader2compressed::~LASpointReader2compressed()
{
	if (dec)
	{
		dec->done();

		delete dec;
		delete ic_dx;
		delete ic_dy;
		delete ic_z;
		delete m_changed_values;
		delete ic_intensity;
		delete m_bit_byte;
		delete m_classification;
		delete ic_scan_angle_rank;
		delete m_user_data;
		delete ic_point_source_ID;
    delete ic_r;
    delete ic_g;
    delete ic_b;
	}
}

}
