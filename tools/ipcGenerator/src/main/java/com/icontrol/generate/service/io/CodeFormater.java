/*
 * Copyright 2021 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package com.icontrol.generate.service.io;

//-----------
//imports
//-----------

import java.io.PrintWriter;
import java.util.ArrayList;

import com.icontrol.util.StringUtils;

//-----------------------------------------------------------------------------
// Class definition:    CodeFormater
/**
 *  Helper class to keep generated code as tabular looking as possible.
 *  Created up front with a number of columns, then populated row-by-row.
 *  When done populating, will print it's contents to an output stream.
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class CodeFormater
{
    int             numCols;
    int             currRow;
    ArrayList<Row>  rows;
    int             max[];
    
    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public CodeFormater(int numColumns)
    {
        // init settings
        //
        numCols = numColumns;
        currRow = -1;
        rows = new ArrayList<Row>();
        max = new int[numCols];
    }

    //--------------------------------------------
    /**
     * Populate the column with this value (0 indexed)
     */
    //--------------------------------------------
    public void setValue(String value, int column)
    {
        // get the row
        //
        Row row = rows.get(currRow);
        
        // apply the data
        //
        row.setCell(column, value);
        
        // see if this is the largest string in that column
        //
        if (value != null)
        {
            int len = value.length();
            if (len > max[column])
            {
                max[column] = len;
            }
        }
    }

    //--------------------------------------------
    /**
     * Moves the pointer to the next row
     */
    //--------------------------------------------
    public void nextRow()
    {
        // move current row to the appropriate offset, then add the new row
        //
        currRow = rows.size();
        rows.add(new Row(numCols));
    }
    
    //--------------------------------------------
    /**
     * Add a filler row
     */
    //--------------------------------------------
    public void appendFillerRow(String line)
    {
        // move current row to the appropriate offset, then add the new row
        //
        currRow = rows.size();
        Row r = new Row(1);
        r.filler = true;
        r.setCell(0, line);
        
        rows.add(r);
    }
    
    //--------------------------------------------
    /**
     * Outputs the buffer to the supplied stream
     */
    //--------------------------------------------
    public void printBuffer(PrintWriter output, int pad)
    {
        // make a padding buffer out front
        //
        StringBuffer prefix = new StringBuffer();
        StringUtils.padStringBuffer(prefix, pad, ' ');
        
        for (int i = 0, count = rows.size() ; i < count ; i++)
        {
            // print this row, first the padding
            //
            Row r = rows.get(i);
            output.print(prefix);
            
            for (int j = 0 ; j < numCols ; j++)
            {
                // Get the cell for this column
                //
                StringBuffer buff = new StringBuffer();
                String val = r.cell(j);
                if (val != null)
                {
                    buff.append(val);
                }
                
                // pad to the max length
                //
                StringUtils.padStringBuffer(buff, max[j], ' ');
                output.print(buff);
                output.print(' ');
            }
            
            // print newline before going to the next row
            //
            output.println();
        }
    }
    
    //--------------------------------------------
    /**
     * Outputs the buffer to the supplied stream
     */
    //--------------------------------------------
    public StringBuffer toStringBuffer(int pad)
    {
        // make a padding buffer out front
        //
        StringBuffer retVal = new StringBuffer();
        StringBuffer prefix = new StringBuffer();
        StringUtils.padStringBuffer(prefix, pad, ' ');
        
        for (int i = 0, count = rows.size() ; i < count ; i++)
        {
            // print this row, first the padding
            //
            Row r = rows.get(i);
            retVal.append(prefix);
            
            if (r.filler == true)
            {
                // filler row, print first cell
                //
                retVal.append(r.cell(0));
                retVal.append('\n');
                continue;
            }
            
            for (int j = 0 ; j < numCols ; j++)
            {
                // Get the cell for this column
                //
                StringBuffer buff = new StringBuffer();
                String val = r.cell(j);
                if (val != null)
                {
                    buff.append(val);
                }
                
                // pad to the max length
                //
                StringUtils.padStringBuffer(buff, max[j], ' ');
                retVal.append(buff);
                retVal.append(' ');
            }
            
            // print newline before going to the next row
            //
            retVal.append('\n');
        }
        
        return retVal;
    }

    
    //-----------------------------------------------------------------------------
    // Class definition:    Row
    /**
     *  Represent a row of strings
     **/
    //-----------------------------------------------------------------------------
    private static class Row
    {
        boolean filler;
        String data[];
        
        Row(int numCols)
        {
            data = new String[numCols];
        }
        
        void setCell(int col, String val)
        {
            data[col] = val;
        }
        
        String cell(int col)
        {
            if (col < data.length)
            {
                return data[col];
            }
            return null;
        }
    }
}



//+++++++++++
//EOF
//+++++++++++