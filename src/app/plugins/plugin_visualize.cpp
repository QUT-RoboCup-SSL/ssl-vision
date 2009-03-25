//========================================================================
//  This software is free: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License Version 3,
//  as published by the Free Software Foundation.
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  Version 3 in the file COPYING that came with this distribution.
//  If not, see <http://www.gnu.org/licenses/>.
//========================================================================
/*!
  \file    plugin_visualize.cpp
  \brief   C++ Implementation: plugin_visualize
  \author  Stefan Zickler, 2008
*/
//========================================================================
#include "plugin_visualize.h"
#include <sobel.h>


PluginVisualize::PluginVisualize(FrameBuffer * _buffer, const CameraParameters& camera_params, const RoboCupCalibrationHalfField& field)
 : VisionPlugin(_buffer), camera_parameters(camera_params), field(field)
{
  _settings=new VarList("Visualization");
  _settings->addChild(_v_enabled=new VarBool("enable", true));
  _settings->addChild(_v_image=new VarBool("image", true));
  _settings->addChild(_v_greyscale=new VarBool("greyscale", true));
  _settings->addChild(_v_thresholded=new VarBool("thresholded", true));
  _settings->addChild(_v_blobs=new VarBool("blobs", true));
  _settings->addChild(_v_camera_calibration=new VarBool("camera calibration", true));
  _settings->addChild(_v_calibration_result=new VarBool("calibration result", true));
  _settings->addChild(_v_detected_edges=new VarBool("detected edges", true));
  _settings->addChild(_v_complete_sobel=new VarBool("complete edge detection", true));
  _v_complete_sobel->setBool(false);
  _threshold_lut=0;
  edge_image = 0;
  temp_grey_image = 0;
}


PluginVisualize::~PluginVisualize()
{
  if(edge_image)
    delete edge_image;
  if(temp_grey_image)
    delete temp_grey_image;
}

VarList * PluginVisualize::getSettings() {
  return _settings;
}

string PluginVisualize::getName() {
  return "Visualization";
}

ProcessResult PluginVisualize::process(FrameData * data, RenderOptions * options)
{
  (void)options;
  if (data==0) return ProcessingFailed;

  VisualizationFrame * vis_frame;
  if ((vis_frame=(VisualizationFrame *)data->map.get("vis_frame")) == 0) {
    vis_frame=(VisualizationFrame *)data->map.insert("vis_frame",new VisualizationFrame());
  }

  if (_v_enabled->getBool()==true) {
    //check video data...
    if (data->video.getWidth() == 0 || data->video.getHeight()==0) {
      //there is no valid video data
      //mark visualization data as invalid
      vis_frame->valid=false;
      return ProcessingOk;
    } else {
      //allocate visualization frame accordingly:
      vis_frame->data.allocate(data->video.getWidth(), data->video.getHeight());
    }

    if (_v_image->getBool()==true) {
      //if converting entire image then blanking is not needed
      ColorFormat source_format=data->video.getColorFormat();
      if (source_format==COLOR_RGB8) {
        //plain copy of data
        memcpy(vis_frame->data.getData(),data->video.getData(),data->video.getNumBytes());
      } else if (source_format==COLOR_YUV422_UYVY) {
        Conversions::uyvy2rgb(data->video.getData(),(unsigned char*)(vis_frame->data.getData()),data->video.getWidth(),data->video.getHeight());
      } else {
        //blank it:
        vis_frame->data.fillBlack();
        fprintf(stderr,"Unable to visualize color format: %s\n",Colors::colorFormatToString(source_format).c_str());
        fprintf(stderr,"Currently supported are rgb8 and yuv422 (UYVY).\n");
        fprintf(stderr,"(Feel free to add more conversions to plugin_visualize.cpp).\n");
      }
      if (_v_greyscale->getBool()==true) {
        unsigned int n = vis_frame->data.getNumPixels();
        rgb * vis_ptr = vis_frame->data.getPixelData();
        rgb color;
        for (unsigned int i=0;i<n;i++) {
          color=vis_ptr[i];
          color.r=color.g=color.b=((color.r+color.g+color.b)/3);
          vis_ptr[i]=color;
        }
      }
    } else {
      vis_frame->data.fillBlack();
    }

    if (_v_thresholded->getBool()==true) {
      if (_threshold_lut!=0) {
        Image<raw8> * img_thresholded=(Image<raw8> *)(data->map.get("cmv_threshold"));
        if (img_thresholded!=0) {
          int n = vis_frame->data.getNumPixels();
          if (img_thresholded->getNumPixels()==n) {
            rgb * vis_ptr = vis_frame->data.getPixelData();
            raw8 * seg_ptr = img_thresholded->getPixelData();
            for (int i=0;i<n;i++) {
              if (seg_ptr[i].getIntensity() !=0) {
                vis_ptr[i]=_threshold_lut->getChannel(seg_ptr[i].getIntensity()).draw_color;
              }
            }
          }
        }
      }
    }

    //draw blob finding results:
    if (_v_blobs->getBool()==true) {
      CMVision::ColorRegionList * colorlist;
      colorlist=(CMVision::ColorRegionList *)data->map.get("cmv_colorlist");
      if (colorlist!=0) {
        CMVision::RegionLinkedList * regionlist;
        regionlist = colorlist->getColorRegionArrayPointer();
        for (int i=0;i<colorlist->getNumColorRegions();i++) {
          rgb blob_draw_color;
          if (_threshold_lut!=0) {
            blob_draw_color= _threshold_lut->getChannel(i).draw_color;
          } else {
            blob_draw_color.set(255,255,255);
          }
          CMVision::Region * blob=regionlist[i].getInitialElement();
          while (blob != 0) {
            vis_frame->data.drawLine(blob->x1,blob->y1,blob->x2,blob->y1,blob_draw_color);
            vis_frame->data.drawLine(blob->x1,blob->y1,blob->x1,blob->y2,blob_draw_color);
            vis_frame->data.drawLine(blob->x1,blob->y2,blob->x2,blob->y2,blob_draw_color);
            vis_frame->data.drawLine(blob->x2,blob->y1,blob->x2,blob->y2,blob_draw_color);
            blob=blob->next;
          }
        }
      }
    }
    //transfer image...optionally applying filtering effects
    
    // Camera calibration 
    if (_v_camera_calibration->getBool()==true) 
    {
      // Principal point
      rgb ppoint_draw_color;
      ppoint_draw_color.set(255,0,0);
      int x = camera_parameters.principal_point_x->getDouble();
      int y = camera_parameters.principal_point_y->getDouble();
      vis_frame->data.drawFatLine(x-15,y-15,x+15,y+15,ppoint_draw_color);
      vis_frame->data.drawFatLine(x+15,y-15,x-15,y+15,ppoint_draw_color);
      // Calibration points
      rgb cpoint_draw_color;
      cpoint_draw_color.set(0,255,255);
      int bx = camera_parameters.additional_calibration_information->left_corner_image_x->getDouble();
      int by = camera_parameters.additional_calibration_information->left_corner_image_y->getDouble();
      vis_frame->data.drawFatBox(bx-5,by-5,11,11,cpoint_draw_color);
      vis_frame->data.drawString(bx-40,by-25,"Left", cpoint_draw_color);
      vis_frame->data.drawString(bx-40,by-15,"Corner", cpoint_draw_color);
      bx = camera_parameters.additional_calibration_information->right_corner_image_x->getDouble();
      by = camera_parameters.additional_calibration_information->right_corner_image_y->getDouble();
      vis_frame->data.drawFatBox(bx-5,by-5,11,11,cpoint_draw_color);
      vis_frame->data.drawString(bx+5,by-25,"Right", cpoint_draw_color);
      vis_frame->data.drawString(bx+5,by-15,"Corner", cpoint_draw_color);
      bx = camera_parameters.additional_calibration_information->left_centerline_image_x->getDouble();
      by = camera_parameters.additional_calibration_information->left_centerline_image_y->getDouble();
      vis_frame->data.drawFatBox(bx-5,by-5,11,11,cpoint_draw_color);
      vis_frame->data.drawString(bx-40,by+15,"Left", cpoint_draw_color);
      vis_frame->data.drawString(bx-40,by+25,"Center", cpoint_draw_color);
      bx = camera_parameters.additional_calibration_information->right_centerline_image_x->getDouble();
      by = camera_parameters.additional_calibration_information->right_centerline_image_y->getDouble();
      vis_frame->data.drawFatBox(bx-5,by-5,11,11,cpoint_draw_color);
      vis_frame->data.drawString(bx+5,by+15,"Right", cpoint_draw_color);
      vis_frame->data.drawString(bx+5,by+25,"Center", cpoint_draw_color);
    }
    
    // Result of camera calibration, draws field to image
    if (_v_calibration_result->getBool()==true) 
    {
      int stepsPerLine(20);
      // Left side line:
      drawFieldLine(camera_parameters.field.left_corner_x->getInt(),
                    camera_parameters.field.left_corner_y->getInt(),
                    camera_parameters.field.left_centerline_x->getInt(),
                    camera_parameters.field.left_centerline_y->getInt(), 
                    stepsPerLine, vis_frame);
      // Right side line:
      drawFieldLine(camera_parameters.field.right_corner_x->getInt(),
                    camera_parameters.field.right_corner_y->getInt(),
                        camera_parameters.field.right_centerline_x->getInt(),
                            camera_parameters.field.right_centerline_y->getInt(), 
                                stepsPerLine, vis_frame);
      // Goal line:
      drawFieldLine(camera_parameters.field.right_corner_x->getInt(),
                    camera_parameters.field.right_corner_y->getInt(),
                        camera_parameters.field.left_corner_x->getInt(),
                            camera_parameters.field.left_corner_y->getInt(),
                                stepsPerLine, vis_frame);
      // Center line:
      drawFieldLine(camera_parameters.field.left_centerline_x->getInt(),
                    camera_parameters.field.left_centerline_y->getInt(),
                        camera_parameters.field.right_centerline_x->getInt(),
                            camera_parameters.field.right_centerline_y->getInt(), 
                                stepsPerLine, vis_frame);


      // Center circle:
      double prev_x = 0;
      double prev_y = 500;
      
      for (double i=0.314; i <= 3.14; i += 0.314)
      {
        double y = cos(i) * 500;
        double x = sin(i) * 500;

        drawFieldLine((int) prev_x,
                      (int) prev_y,
                      (int) x,
                      (int) y, 
                      stepsPerLine, vis_frame);
        
        prev_x = x;
        prev_y = y;
      }
      
      // Goal area:
      prev_x = 0;
      prev_y = -500;
      
      for (double i=3.14; i<=3.14+3.14/2; i+= 0.314)
      {
        double y = cos(i) * 500;
        double x = sin(i) * 500;
        
        drawFieldLine((int) prev_x + 3025,
                      (int) prev_y - 175,
                      (int) x + 3025,
                      (int) y - 175, 
                      stepsPerLine, vis_frame);
        
        prev_x = x;
        prev_y = y;
      }
      
      drawFieldLine(2525, -175, 2525, 175, stepsPerLine, vis_frame);       
      
      prev_x = -500;
      prev_y = 0;
      
      for (double i=3.14+3.14/2; i<=3.14+3.14; i+= 0.314)
      {
        double y = cos(i) * 500;
        double x = sin(i) * 500;
        
        drawFieldLine((int) prev_x + 3025,
                      (int) prev_y + 175,
                      (int) x + 3025,
                      (int) y + 175, 
                      stepsPerLine, vis_frame);

        prev_x = x;
        prev_y = y;
      }  

      
      for (int grid_y=0; grid_y < 2025; grid_y += 500)
      {
        drawFieldLine((int) 0,
                      (int) -grid_y,
                      (int) 3025,
                      (int) -grid_y, 
                      stepsPerLine, vis_frame);
        
        drawFieldLine((int) 0,
                      (int) grid_y,
                      (int) 3025,
                      (int) grid_y, 
                      stepsPerLine, vis_frame);
        
      }
      for (int grid_x=0; grid_x < 3025; grid_x += 500)
      {
        drawFieldLine((int) grid_x,
                      (int) -2025,
                      (int) grid_x,
                      (int) 2025, 
                      stepsPerLine, vis_frame);
      } 
    }
    
    // Test edge detection for calibration
    if (_v_complete_sobel->getBool()==true) 
    {
      if(edge_image == 0)
      { 
        edge_image = new greyImage(data->video.getWidth(),data->video.getHeight());
        temp_grey_image = new greyImage(data->video.getWidth(),data->video.getHeight());
      }
      Images::convert(vis_frame->data, *temp_grey_image);
      // Draw sobel image: Contrast towards more brightness is painted white,
      //                   Contrast towards more darkness is painted green
      for(int y=1; y<temp_grey_image->getHeight()-1; ++y)
      {
        for(int x=1; x<temp_grey_image->getWidth()-1; ++x)
        {
          int colVB,colVD,colHB,colHD;
          colVB = Sobel::verticalBrighter(*temp_grey_image,x,y,30);
          colVD = Sobel::verticalDarker(*temp_grey_image,x,y,30);
          colHB = Sobel::horizontalBrighter(*temp_grey_image,x,y,30);
          colHD = Sobel::horizontalDarker(*temp_grey_image,x,y,30);
          int dMax = colVD > colHD ? colVD : colHD;
          int bMax = colVB > colHB ? colVB : colHB;
          grey col;
          if(dMax > bMax)
            col.v = 1;
          else if (bMax > dMax)
            col.v = 2;
          else
            col.v = 0;
          edge_image->setPixel(x,y,col);
        }
      }
      unsigned int n = edge_image->getNumPixels();
      rgb * vis_ptr = vis_frame->data.getPixelData();
      grey * edge_ptr = edge_image->getPixelData();
      rgb color;
      for (unsigned int i=0;i<n;i++) {
        unsigned char col = edge_ptr[i].v;
        if(col == 0)
        {
          color.r=color.g=color.b=col;
        }
        else if(col == 1)
        {
          color.r=0; color.g=255; color.b=0;
        }
        else if(col == 2)
        {
          color.r=255; color.g=255; color.b=255;
        }
        vis_ptr[i]=color;
      }
    }
    
    // Result of edge detection for second calibration step
    if (_v_detected_edges->getBool()==true) 
    {
//       if(edge_image == 0)
//       {
//         edge_image = new greyImage(data->video.getWidth(),data->video.getHeight());
//         temp_grey_image = new greyImage(data->video.getWidth(),data->video.getHeight());
//       }
//       Images::convert(vis_frame->data, *temp_grey_image);
//       for(int i=0; i<300; i+= 50)
//       {
//         int maxEdgeX = Sobel::maximumHorizontalEdge(*temp_grey_image, 100+i, 5, 200,30, Sobel::horizontalBrighter);
//         if(maxEdgeX > -1)
//         {
//           rgb edge_draw_color;
//           edge_draw_color.set(255,0,0);
//           vis_frame->data.drawBox(maxEdgeX-5,100+i-5,11,11,edge_draw_color);
//           vis_frame->data.drawLine(maxEdgeX,100+i-2,maxEdgeX,100+i+2,edge_draw_color);
//         }
//       }
//       for(int i=0; i<300; i+= 50)
//       {
//         int maxEdgeY = Sobel::maximumVerticalEdge(*temp_grey_image, 100+i, 30, 100,30, Sobel::verticalBrighter);
//         if(maxEdgeY > -1)
//         {
//           rgb edge_draw_color;
//           edge_draw_color.set(255,0,0);
//           vis_frame->data.drawBox(100+i-5,maxEdgeY-5,11,11,edge_draw_color);
//           vis_frame->data.drawLine(100+i-2,maxEdgeY,100+i+2,maxEdgeY,edge_draw_color);
//         }
//       }
      rgb edge_draw_color;
      edge_draw_color.set(255,0,0);
      for(unsigned int ls=0; ls<camera_parameters.line_segment_data.size(); ++ls)
      {
        const CameraParameters::LSCalibrationData& segment =
            camera_parameters.line_segment_data[ls];
        for(unsigned int edge=0; edge<segment.pts_on_line.size(); ++edge)
        {
          const GVector::vector2d<double>& pt = segment.pts_on_line[edge];
          vis_frame->data.drawBox(pt.x-5,pt.y-5,11,11,edge_draw_color);
          if(segment.horizontal)
            vis_frame->data.drawLine(pt.x,pt.y-2,pt.x,pt.y+2,edge_draw_color);
          else
            vis_frame->data.drawLine(pt.x-2,pt.y,pt.x+2,pt.y,edge_draw_color);
        }
      }
    }

/*    rgb edge_draw_color;
    edge_draw_color.set(255,0,0);
    vis_frame->data.drawChar(100,100,'C', edge_draw_color);
    vis_frame->data.drawString(200,200,"Supertoller String!", edge_draw_color);*/
    
    vis_frame->valid=true;
  } else {
    vis_frame->valid=false;
  }
  return ProcessingOk;
}

void PluginVisualize::setThresholdingLUT(LUT3D * threshold_lut) {
  _threshold_lut=threshold_lut;
}

void PluginVisualize::drawFieldLine(double xStart, double yStart, 
                                    double xEnd, double yEnd, 
                                    int steps, VisualizationFrame * vis_frame)
{
  GVector::vector3d<double> start(xStart, yStart,0);
  GVector::vector3d<double> end(xEnd, yEnd,0);
  GVector::vector3d<double> offset;
  offset = (end - start);
  offset *= 1.0/steps;
  GVector::vector2d<double> lastInImage;
  GVector::vector3d<double> lastInWorld(start);
  camera_parameters.field2image(lastInWorld, lastInImage);
  for(int i=0; i<steps; ++i)
  {
    GVector::vector3d<double> nextInWorld = lastInWorld + offset;
    GVector::vector2d<double> nextInImage;
    camera_parameters.field2image(nextInWorld, nextInImage);
    //    std::cout<<"Point in image: "<<posInImage.x<<","<<posInImage.y<<std::endl;
    rgb draw_color;
    draw_color.set(255,100,100);
    vis_frame->data.drawFatLine(lastInImage.x,lastInImage.y,
                                nextInImage.x,nextInImage.y,draw_color);
    lastInWorld = nextInWorld;
    lastInImage = nextInImage;
  }
}