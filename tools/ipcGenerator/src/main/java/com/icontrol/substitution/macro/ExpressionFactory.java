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

package com.icontrol.substitution.macro;

//-----------
//imports
//-----------

import java.io.BufferedReader;
import java.io.IOException;
import java.io.StreamTokenizer;
import java.io.StringReader;
import java.util.ArrayList;
import java.util.List;

//-----------------------------------------------------------------------------
// Class definition:    ExpressionFactory
/**
 *  Singleton parser to provide a list of Expression or Token objects
 *  This is intended to parse 'macro expressions' such as #if (...) 
 *  The returned MacroExpressions should have their 'contents' attached to them
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class ExpressionFactory
{
	public static final String	IF_MARKER		= "#if";
	public static final String	ELSE_MARKER		= "#else";
	public static final String	ELSE_IF_MARKER	= "#elseif";
	public static final String	END_MARKER		= "#end";
	
	//--------------------------------------------
	/**
	 * Given a string that contains a "#if" and a "#end", will produce
	 * a Macro that can be evaluated to see which content to utilize.
	 * 
	 * @see Macro
	 */
	//--------------------------------------------
	public static Macro parseMacro(String buffer)
		throws InvalidMacroException, IOException
	{
		Macro retVal = new Macro();
		
		// make sure we have 'Unix' style line ends.  We don't want
		// to deal with Winblows style (CR + LF) since we're calculating
		// string lengths and need to assume a single new-line char
		//
//		buffer = StringUtils.convertToUnix(buffer);
		
		// look for the beginning of a macro in 'buffer'
		//
		int start = buffer.indexOf(IF_MARKER);
		if (start < 0)
		{
			// no #if, so there isn't a macro in this buffer
			//
			return null;
		}
		retVal.setSpanStart(start);
		
		// start reading lines from this point.  We'll be looking for
		// other 'markers' and creating MacroExpressions of each
		//
		boolean hasSub = false;
		MacroReader reader = null;
		try
		{
			int loc = start;
			reader = new MacroReader(new StringReader(buffer.substring(start)));
			
			// read lines until we find a 'start'
			//
			MacroExpression exp = null;
			String line = null;
			while ((line = reader.readLine()) != null)
			{
				// update our current location.  NOTE: we'll add 1 more 
				// for the EOLN char removed from the readLine() above.
				//
				int lineLen = line.length();
				if (reader.isMulti() == false)
				{
					lineLen++;
				}
				loc += lineLen;
				
				// trim and to-lower the line
				//
				String trim = line.trim();
				if (trim.startsWith("#") == true)
				{
					// got one of our markers.  Make it lower case so we can see which one it is
					//
					trim = trim.toLowerCase();
					
					if (trim.startsWith(IF_MARKER) == true)
					{
						// at 'start', make sure we didn't already find a 'start'
						// and if we did, we're in a sub-macro situation
						//
						int orig = (loc - lineLen);
						if (orig != start)
						{
							// in a sub-macro, so read it into a Macro
							// just so we can get the '#end' location
							//
							Macro sub = parseMacro(buffer.substring(orig));
							if (sub != null)
							{
								// grab everything from 'orig' to 'sub.spanEnd'
								// and append to the current 'expression'.  This sub-expression
								// will be re-evaluated if the owning macro passes.
								//
								hasSub = true;
								// ensure that we get the new line 
								int end = sub.getSpanEnd() +1;
								String skip = buffer.substring(orig, orig+end);
								if (exp != null)
								{
									exp.appendAttachment(skip);
								}
								
								// have the reader skip over the bytes we just processed
								// (remember that we backed up the 'begin' to include the line already read)
								//  
//								int diff = end - (line.length() + 1);
//								reader.skip(diff);
								
								// now remove the count for the new line because it is needed for further processing
								int diff = ( skip.length() - 1 ) - line.length();
								reader.skip(diff);
								loc += diff;
							}
						}
						else
						{
							// parse this line as a MacroExpression
							//
							exp = parseMacroLine(line);
							retVal.addExpression(exp);
						}
					}
					else if (trim.startsWith(ELSE_IF_MARKER) == true || trim.startsWith(ELSE_MARKER) == true)
					{
						// parse this line as a MacroExpression
						//
						exp = parseMacroLine(line);
						retVal.addExpression(exp);
					}
					else if (trim.equals(END_MARKER) == true)
					{
						// parse this line as a MacroExpression
						//
						exp = parseMacroLine(line);
						retVal.addExpression(exp);

						// save the 'end' location of the macro
						//
						int stop = loc;
						if (reader.isMulti() == false && hasSub == false)
						{
							// if the macro has sub-macros in it, then this would be pointing to the \n after the #end
							// and we don't want to pull in the 'd' character
							//
							stop--;
						}
						retVal.setSpanEnd(stop);
//						if (logger.isDebugLevel(ExpressionFactory.class) == true)
//						{
//							// show the complete contents the macro spans
//							//
//							String span = buffer.substring(start, stop);
//							logger.debug(ExpressionFactory.class, "\nParsed macro\n\nmacro=\n===============\n"+retVal+"\nderived from\n===============\n"+span);
//						}
						break;
					}
					
					// already processed this line, so go to the next one
					//
					continue;
				}
		
				// adding content to the last 'expression' we parsed
				//
				if (exp != null)
				{
					exp.appendAttachment(line);
				}
			}
			
			reader.close();
			reader = null;
		}
		finally
		{
			if (reader != null)
			{
				reader.close();
			}
		}
		
		// before returning, make sure it has all the parts it needs
		//
		retVal.validateMacro();
		return retVal;
	}
	
	//--------------------------------------------
	/**
	 * Given a single macro line (#if, #elseif), this will 
	 * parse it into a MacroExpression.
	 */
	//--------------------------------------------
	public static MacroExpression parseMacroLine(String line)
		throws InvalidMacroException
	{
		// get Tokens for the line
		//
		ArrayList<Token> tokens = parseTokens(line);
		if (tokens.size() == 0)
		{
			// invalid macro
			//
			throw new InvalidMacroException(line);
		}
		
		// peek at the first Token from the list.  See if it's a 'command'
		// that contains our MacroType.  Otherwise assume 'IF' to deal
		// with single 'tests' that Reports and Apply-Link objects run.
		//
		MacroType macroType = MacroType.IF;
		Token first = tokens.get(0);
		if (first.getKind() == Token.Type.command)
		{
			// see if a valid MacroType of operation
			//
			String cmd = first.getValue();
			macroType = MacroType.getForCommand(cmd);
			if (macroType == null)
			{
				InvalidMacroException ex = new InvalidMacroException("Un-recognized macro command '#"+cmd+"'");
				ex.setMacroLine(line);
				throw ex;
			}
			
			// skip this Token 
			//
			tokens.remove(0);
		}
		
		// create the container to return
		//
		MacroExpression retVal = new MacroExpression(macroType);
		
		// walk through our Tokens and place into Expression containers
		//
		int size = tokens.size();
		if (size == 0)
		{
			// it's possible we have an #else or #end
			//
			if (macroType != MacroType.ELSE && macroType != MacroType.END)
			{
				InvalidMacroException ex = new InvalidMacroException("Nothing to process for macro '#"+macroType.toString()+"'");
				ex.setMacroLine(line);
				throw ex;
			}
			
			return retVal;
		}
		
		try
		{
			// make the expression from the remaining tokens
			//
			Expression exp = createExpression(tokens);
			retVal.setExpression(exp);

			return retVal;
		}
		catch (InvalidMacroException ex)
		{
			// put the macro into the exception so we log what string we had an error in
			//
			ex.setMacroLine(line);
			throw ex;
		}
	}

	//--------------------------------------------
	/**
	 * Constructor for this class
	 */
	//--------------------------------------------
	private ExpressionFactory()
	{
	}
	
	//--------------------------------------------
	/**
	 * 
	 */
	//--------------------------------------------
	private static Expression createExpression(List<Token> tokens)
		throws InvalidMacroException
	{
		// make a list of Expression and TokenOperator objects
		// to preserve the order of things we create.  This is used
		// at the end to combine into complex expressions
		//
		ArrayList<Object> queue = new ArrayList<Object>();
		
		// syntax for an Expression using a set of Tokens:
		// 
		//  not-comparison [ group_start | variable ] 				ex: !${x}   !(...)
		//  variable 												ex: ${x} 
		//  [ variable | value ] comparison [ variable | value ]	ex: ${x} > 10   10 == ${x}
		//  [ group_start ] [ ... ] [ group_end ] [ and | or ... ]	ex: ( ${x} > 10 )   ( !${x} && 10 > 2 )   ${x} == "yes" && (${y} != "no")
		//  
		for (int i = 0, count = tokens.size() ; i < count ; i++)
		{
			// get the current token and it's type
			//
			Token curr = tokens.get(i);
			Token.Type currType = curr.getKind();
			
			// peek at the next token since we may need it
			//
			Token next = null;
			Token.Type nextType = null;
			if (i+1 < count)
			{
				next = tokens.get(i+1);
				nextType = next.getKind();
			}
			
			// look for syntax #1:
			// not-comparison [ group_start | variable ]
			//
			if (curr.isNegateComparison() == true)
			{
				// have a not (!) token.  peek at the next token to see if
				// it is a 'variable' or a 'group_start'
				// 
				if (next == null || (nextType != Token.Type.group_start && nextType != Token.Type.variable))
				{
					throw new InvalidMacroException("Expected '(' or '${var}' after '!'");
				}
					
				if (nextType == Token.Type.variable)
				{
					// if dealing with a variable, make a VariableExpression
					//
					Expression exp = new MacroExpression.VariableExpression(next, true);
					queue.add(exp);
					
					// skip 'next' since it was processed
					//
					i++;
				}
				else
				{
					// otherwise, got a sub-group, so make that then add the not in front
					// note that we skip 'next' since we already know it's a group_start
					//
					List<Token> subList = getSubgroup(tokens, i+2);
					if (subList == null || subList.size() == 0)
					{
						throw new InvalidMacroException("Missing ')'");
					}
					Expression group = createExpression(subList);
					
					// wrap our group in a NegateExpression since we need to flip the response value
					//
//					queue.add(group);
					queue.add(new MacroExpression.NegateExpression(group));
						
					// skip over the group (including the start/end markers)
					//
					i += subList.size() + 2;
				}
			}
			
			// look for syntax #2:
			//  variable		ex: ${x} 
			//
			else if (currType == Token.Type.variable && (next == null || nextType == Token.Type.group_start || next.isOperator() || next.isNegateComparison()))
			{
				// got a 'variable' and the next token is null, (, &&, ||, or !
				//
				Expression exp = new MacroExpression.VariableExpression(curr, false);
				queue.add(exp);
			}

			// look for syntax #3:
			//  [ variable | value ] comparison [ variable | value ]	ex: ${x} > 10   10 == ${x}
			//
			else if ((currType == Token.Type.variable || currType == Token.Type.value) && (next != null && next.isComparison()))
			{
				// got a 'variable' and the next token is a comparator
				// get the Token after 'next' and make sure it's a 'variable' or 'value'
				//
				Token third = null;
				if (i+2 < count)
				{
					third = tokens.get(i+2);
				}
				if (third == null || (third.getKind() != Token.Type.variable && third.getKind() != Token.Type.value))
				{
					throw new InvalidMacroException("Expecting 'variable' or 'value' after "+next.getValue());
				}
				
				// good to go
				//
				Expression exp = new MacroExpression.TokenExpression(curr, (TokenComparison)next, third);
				queue.add(exp);
				
				// skip the compare & third token
				//
				i += 2;
			}
			
			// look for syntax #4:
			//  [ group_start ] [ ... ] [ group_end ] [ and | or ... ]	ex: ( ${x} > 10 )   ( !${x} && 10 > 2 )   ${x} == "yes" && (${y} != "no")
			//
			else if (currType == Token.Type.group_start)
			{
				// got a sub-group. note that we skip 'curr' since we already know it's a group_start
				//
				List<Token> subList = getSubgroup(tokens, i+1);
				if (subList == null || subList.size() == 0)
				{
					throw new InvalidMacroException("Missing ')'");
				}
				Expression group = createExpression(subList);
				queue.add(group);
					
				// skip over the group (including the start/end markers)
				//
				i += subList.size() + 1;
			}
			
			// look for Operand (&&, ||) 
			//
			else if (curr.isOperator() == true)
			{
				// just add the token to the queue
				//
				queue.add(curr);
			}
			
			else
			{
				// error?
				//
				throw new InvalidMacroException("Malformed expression");
			}
		}
		
		// now that we have our queue of Expressions and Operators,
		// combine them into AND/OR expressions
		//
		int size = queue.size();
		if (size == 0)
		{
			// nothing found
			//
			return null;
		}
		else if (size == 1)
		{
			// simple expression with 1 element
			//
			Object first = queue.get(0);
			if (first instanceof Expression)
			{
				return (Expression)first;
			}
			else
			{
				// cannot start with an operator  
				//
				throw new InvalidMacroException("Malformed expression");
			}
		}
		
		// the list must be an odd number since we're expecting
		//  expression, operator, expression [operator, expression]...
		//
		if ((size % 2) != 1)
		{
			throw new InvalidMacroException("Malformed expression");
		}
			
		// Loop through the list.  We're assuming everything is in sets of 2-3
		//
		MacroExpression.ComplexExpression head = null;
		MacroExpression.ComplexExpression lastExp = null;
		for (int i = 0 ; i < size ; i++)
		{
			Object exp = queue.get(i);
			Object op = null;
			if (i+1 < size)
			{
				op = queue.get(i+1);
			}
			
			// look for Expression + Operator 
			//
			if (exp instanceof Expression && (op != null && op instanceof TokenOperator))
			{
				// make the AND or OR expression based on the Token
				//
				MacroExpression.ComplexExpression curr = null;
				TokenOperator tok = (TokenOperator)op;
				if (tok.getOperator() == TokenOperator.Operator.and)
				{
					// make the AND expression and leave the 'right' empty for now
					//
					curr = new MacroExpression.AndExpression((Expression)exp);
				}
				else
				{
					// make the AND expression and leave the 'right' empty for now
					//
					curr = new MacroExpression.OrExpression((Expression)exp);
				}
				
				// see if we're appending to the 'last' expression made
				//
				if (lastExp != null)
				{
					lastExp.setRight(curr);
				}
				lastExp = curr;
				if (head == null)
				{
					head = lastExp;
				}
				
				// skip the operator
				//
				i++;
			}
			
			// look for this being the last thing in the list
			//
			else if (exp instanceof Expression && op == null)
			{
				if (lastExp == null)
				{
					throw new InvalidMacroException("Malformed expression");
				}
				
				lastExp.setRight((Expression)exp);
			}
		}
		
		return head;
	}
	
	//--------------------------------------------
	/**
	 * 
	 */
	//--------------------------------------------
	private static List<Token> getSubgroup(List<Token> list, int start)
	{
		int c = 1;
		for (int x = start, count = list.size() ; x < count ; x++)
		{
			Token curr = list.get(x);
			Token.Type currType = curr.getKind();
			
			if (currType == Token.Type.group_start)
			{
				// got a sub-sub-group, so skip over that
				//
				c++;
			}
			else if (currType == Token.Type.group_end)
			{
				// got a group end marker, see if this is the end
				// of the group we're looking for and not the end
				// of a sub-sub-group
				//
				c--;
				if (c < 1)
				{
					return list.subList(start, x);
				}
			}
		}	
		
		// never found the end
		//
		return null;
	}
	
	//--------------------------------------------
	/**
	 * 
	 */
	//--------------------------------------------
	private static Token createToken(Token.Type kind, String value)
	{
		// look for special case tokens
		//
		if (kind == Token.Type.comparison)
		{
			// see if Comparison or Operator
			//
			if (TokenOperator.Operator.forDesc(value) != null)
			{
				// got an '&&' or a '||'
				//
				return new TokenOperator(kind, value);
			}
			else if (TokenComparison.Comparison.forDesc(value) != null)
			{
				// got a comparator (>, ==, etc)
				//
				return new TokenComparison(kind, value);
			}
		}
		
		// assume this is a general Token
		//
		return new Token(kind, value);
	}
	
	//--------------------------------------------
	/**
	 * Given a macro line (#if, #elseif), this will parse it
	 * into a series of Expression objects.
	 */
	//--------------------------------------------
	private static ArrayList<Token> parseTokens(String line)
	{
		// break-up the line into lexicals
		//
		ArrayList<Token> retVal = new ArrayList<Token>();
		ArrayList<String> lex = getLexicals(line);
		
		// create a temporary a state machine to organize into 
		//
		Buffer last = new Buffer();
		
		// loop through the lexicals to create Token objects
		//
		for (int i = 0, count = lex.size() ; i < count ; i++)
		{
			String curr = lex.get(i);
			if (last.complete == true)
			{
				// create a new Token object now that this buffer is complete
				//
				retVal.add(createToken(last.state, last.buff.toString()));
				last = new Buffer();
			}
			
			// look for special markers
			//
			if (last.analyze(curr) == true)
			{
				continue;
			}
			
			// add the word or number to buffer
			//
			last.append(curr);
		}
		
		// append the last token 
		//
		retVal.add(createToken(last.state, last.buff.toString()));
		
		return retVal;
	}
	
	//--------------------------------------------
	/**
	 * Convert the 'line' into a list of lexicals
	 */
	//--------------------------------------------
	private static ArrayList<String> getLexicals(String line)
	{
		ArrayList<String> retVal = new ArrayList<String>();
		
		// place in a StringTokenizer and get all of the tokens
		//
		StreamTokenizer tokens = null;
		try
		{
			// setup the tokenizer to not be so 'code centric'
			//
			StringReader reader = new StringReader(line);
			tokens = new StreamTokenizer(reader);
			tokens.slashSlashComments(false);
			tokens.slashStarComments(false);
			tokens.lowerCaseMode(false);
			tokens.ordinaryChar('/');
			while (tokens.nextToken() != StreamTokenizer.TT_EOF)
			{
				// add the current token
				//
				if (tokens.ttype == StreamTokenizer.TT_WORD && tokens.sval != null)
				{
					// character string
					//
					retVal.add(tokens.sval);
				}
				else if (tokens.ttype == StreamTokenizer.TT_NUMBER)
				{
					// numeric value
					//
					retVal.add(Double.toString(tokens.nval));
				}
// Changed due to Sonar review - DE7539
//				else if (tokens.ttype == StreamTokenizer.TT_EOL)
//				{
//					// eoln
//				}
				else if (tokens.ttype != StreamTokenizer.TT_EOL)
				{
					// see if 'sval' is not null.  If so, then add the word
					// 
					if (tokens.sval != null)
					{
						retVal.add(tokens.sval);
					}
					else
					{
						// assume special character
						//
						char ch = (char)tokens.ttype;
						String chStr = Character.toString(ch);
						
						// see if should be combined with the last thing we added to 
						// create a Comparison or Operator
						//
						if (retVal.size() > 0)
						{
							int len = retVal.size() - 1;
							String last = retVal.get(len);
							if ( (last.equals("$") == true && ch == '{') ||		// variable ${xx}
								 (last.equals("&") == true && ch == '&') ||		// operand &&
								 (last.equals("|") == true && ch == '|'))		// operand ||
							{
								// replace last with last + current
								//
								retVal.remove(len);
								retVal.add(last + chStr);
								continue;
							}
							else if (ch == '=' && (last.equals("!") || last.equals(">") || last.equals("<") || last.equals("=")))
							{
								// got an equality operand !=, >=, <=, ==
								// so replace last with last + current
								//
								retVal.remove(len);
								retVal.add(last + chStr);
								continue;
							}
						}
						retVal.add(chStr);
					}
				}
			}
		}
		catch (IOException ex)
		{
			ex.printStackTrace();
		}
		
		return retVal;
	}
	
	//-----------------------------------------------------------------------------
	// Class definition:    Buffer
	/**
	 *  Buffer of characters and 'state' while creating Tokens
	 **/
	//-----------------------------------------------------------------------------
	private static class Buffer
	{
		Token.Type		state;
		boolean			complete;
		StringBuffer	buff;
		
		//--------------------------------------------
		/**
		 * 
		 */
		//--------------------------------------------
		Buffer()
		{
			state = Token.Type.value;
			buff = new StringBuffer();
			complete = false;
		}
		
		//--------------------------------------------
		/**
		 * 
		 */
		//--------------------------------------------
		void append(String str)
		{
			// examine our state and mark ourself as complete if necessary
			//
			switch (state)
			{
				case command:
				case value:
					complete = true;
					break;
			}
			
			buff.append(str);
		}
		
		//--------------------------------------------
		/**
		 * 
		 */
		//--------------------------------------------
		boolean analyze(String str)
		{
			if (str.equals("#") == true)
			{
				// start of a Macro marker (if, else, etc)
				//
				state = Token.Type.command;
				return true;
			}
			else if (str.equals("(") == true)
			{
				if (state == Token.Type.variable)
				{
					// method parameters to a variable
					//
					buff.append(str);
				}
				else 
				{
					// begin of a new group
					//
					buff.append(str);
					state = Token.Type.group_start;
					complete = true;
				}
				
				return true;
			}
			else if (str.equals(")") == true)
			{
				if (state == Token.Type.variable)
				{
					// method parameters to a variable
					//
					buff.append(str);
				}
				else 
				{
					// end of the current group
					//
					buff.append(str);
					state = Token.Type.group_end;
					complete = true;
				}

				return true;
			}
			else if (str.equals("${") == true)
			{
				// start of a variable
				//
				state = Token.Type.variable;
				return true;
			}
			else if (str.equals("}") == true)
			{
				if (state == Token.Type.variable)
				{
					// end marker of the variable
					//
					complete = true;
					return true;
				}
				else 
				{
					// not part of a variable, so add to output
					//
					return false;
				}
			}
			else if (str.equals("||") == true || str.equals("&&") == true ||
					 str.equals("!=") == true || str.equals("!") == true ||
					 str.equals("==") == true ||
					 str.equals(">=") == true || str.equals(">") == true ||
					 str.equals("<=") == true || str.equals("<") == true)
			{
				// got a Comparison or Operator
				//
				state = Token.Type.comparison;
				complete = true;
				buff.append(str);
				return true;
			}
			
			// not one of our special characters
			//
			return false;
		}

		//--------------------------------------------
		/**
		 * @see java.lang.Object#toString()
		 */
		//--------------------------------------------
		@Override
		public String toString()
		{
			return state + " - "+buff;
		}
	}
	
	//-----------------------------------------------------------------------------
	// Class definition:    MacroReader
	/**
	 *  Read from a StringReader until EOLN or a #else, #elseif, or #end is found
	 **/
	//-----------------------------------------------------------------------------
	private static class MacroReader
	{
		private BufferedReader		reader;		// helper 
		private ArrayList<String>	buffered;	// lines previously read in 
		private boolean				isMulti;
		
		//--------------------------------------------
		/**
		 * 
		 */
		//--------------------------------------------
		MacroReader(StringReader owner)
		{
			reader = new BufferedReader(owner);
			buffered = new ArrayList<String>();
			isMulti = false;
		}

		//--------------------------------------------
		/**
		 * 
		 */
		//--------------------------------------------
		String readLine() throws IOException
		{
			// first see if we have a line from before to return
			//
			if (buffered.size() > 0)
			{
				// pop off the fist thing in the buffered set
				// and return it
				//
				return pop();
			}
			
			// read a line from the helper reader
			//
			isMulti = false;
			String line = reader.readLine();
			if (line == null)
			{
				// hit eof
				//
				buffered.clear();
				return null;
			}
			
			// see if there are multiple markers in the single line
			// (ex:  #if (x) 0 #else 1 #end )
			//
			if (hasMarkers(line) == true)
			{
				// break the line into sections.  Using the example above,
				// a line for the "#if (x)", "0", "#else", "1", and "#end"
				//
				isMulti = true;
				breakUpSection(line);
				if (buffered.size() > 0)
				{
					// pop and return the first
					//
					return pop();
				}
				
				// something went wrong if we didn't find data from the breakUpSection() call
				//
				throw new IOException("Malformed Expression");
			}
			
			// nothing special, so return the line
			//
			return line;
		}
		
		
		//--------------------------------------------
		/**
		 * 
		 */
		//--------------------------------------------
		boolean isMulti()
		{
			return isMulti;
		}
		
		//--------------------------------------------
		/**
		 * 
		 */
		//--------------------------------------------
		void skip(int amount) throws IOException
		{
			// if we're a multi-line reader, chances are we just read a sub-macro
			// and therefore don't actually 'skip' the reader.  Just purge 'buffered'
			// until we get to "#end"
			//
			if (isMulti == false)
			{
				// do the skip
				//
				reader.skip(amount);
			}
			else
			{
				// pop from buffered until we hit 'end'
				//
				String test = pop();
				while (test != null && test.equals(END_MARKER) == false)
				{
					test = pop();
				}
				
				// see if our buffer is empty and reset our flag if it is
				//
				if (buffered.size() == 0)
				{
					isMulti = false;
				}
			}
		}

		//--------------------------------------------
		/**
		 * 
		 */
		//--------------------------------------------
		void close() throws IOException
		{
			if (reader != null)
			{
				reader.close();
			}
			reader = null;
			buffered = null;
		}
		
		//--------------------------------------------
		/**
		 * 
		 */
		//--------------------------------------------
		private String pop()
		{
			String line = buffered.get(0);
			buffered.remove(0);
			
			return line;
		}

		//--------------------------------------------
		/**
		 * 
		 */
		//--------------------------------------------
		private boolean hasMarkers(String line)
		{
			// remove leading spaces from the line
			//
			line = line.trim();
			
			// first look for the line starting with #
			//
			if (line.startsWith("#") == false)
			{
				return false;
			}
			
			// see if there is another # in this line
			//
			if (line.indexOf('#', 1) == -1)
			{
				// only one #something
				//
				return false;
			}
			
			// break the line into tokens and count the number of 'commands' we find
			// Seems like overkill, but safer then doing "indexOf" due to the fact that
			// #else if a subset of #elseif
			//
			int count = 0;
			ArrayList<Token> tokens = parseTokens(line);
			for (int i = 0 ; i < tokens.size() ; i++)
			{
				Token curr = tokens.get(i);
				if (curr.getKind() == Token.Type.command)
				{
					count++;
					if (count > 1)
					{
						// no need to keep going
						//
						break;
					}
				}
			}
			
			// if 2 or more of these were valid, return true
			//
			if (count > 1)
			{
				return true;
			}
			return false;
		}
		
		//--------------------------------------------
		/**
		 * 
		 */
		//--------------------------------------------
		private void breakUpSection(String line) throws IOException
		{
			// remove previously buffered rows
			//
			buffered.clear();
//			line = line.trim();
			
			// assume we're starting with a marker (since we're only here if hasMarkers() == true)
			// So go ahead and get the first 2 characters
			//
			int start = 0;
			while (start < line.length())
			{
				// find the next # marker
				//
				int end = line.indexOf('#', start+1);
				if (end < 0)
				{
					// must be on a different line, so just use everything from start to end
					//
					String remain = line.substring(start);
					
					// check for possibility of #end something-else
					//
					if (remain.startsWith(END_MARKER) && remain.length() > END_MARKER.length())
					{
						buffered.add(END_MARKER);
						buffered.add(remain.substring(END_MARKER.length()));
					}
					else
					{
						buffered.add(remain);
					}
					return;
				}
				
				// grab everything from start - end
				//
				String section = line.substring(start, end);
				start = end;
				
				// look for #if or #elseif
				//
				if (section.startsWith(IF_MARKER) == true || section.startsWith(ELSE_IF_MARKER) == true)
				{
					// read chars to match parenthesis, for example:  #if ((x) && y)
					//
					int parenStart = section.indexOf('(');
					if (parenStart < 0)
					{
						throw new IOException("Malformed macro section '"+section+"'");
					}
					int parenEnd = corresponding(section);
					if (parenEnd < 0)
					{
						throw new IOException("Malformed macro section '"+section+"'");
					}
					
					// everything between 0 and 'parenEnd' is our macro
					// the rest if the crap to use if the macro passes
					//
					String macro = section.substring(0, parenEnd + 1);
					String remain = section.substring(parenEnd + 1);
					
					buffered.add(macro);
					buffered.add(remain);
				}
				
				// if the 'section' is "#else", then everything after the else gets treated as another line
				//
				else if (section.startsWith(ELSE_MARKER) == true)
				{
					// add the #else
					//
					buffered.add(ELSE_MARKER);
					if (section.length() > ELSE_MARKER.length())
					{
						// add everything after the else (between the #else and #end)
						//
						String remainElse = section.substring(ELSE_MARKER.length());
						buffered.add(remainElse);
						continue;
					}
				}
				
				// look for #end
				//
				else if (section.startsWith(END_MARKER) == true)
				{
					// just slap the end in there 
					//
					buffered.add(END_MARKER);
					
					// DO NOT RETURN HERE, there could be more then 1 macro defined on this line
					//
					//return;
				}
				
				else
				{
					// must be crap in-between (such as leading spaces)
					//
					buffered.add(section);
				}
			}
		}
		
		//--------------------------------------------
		/**
		 *  
		 */
		//--------------------------------------------
		private int corresponding(String input)
		{
			int len = input.length();
			int x = 0;
			int start = 0;
			while (start < len)
			{
				char ch = input.charAt(start);
				if (ch == '(')
				{
					// found ( inside the string, increment the marker and continue on
					//
					x++;
				}
				else if (ch == ')')
				{
					// found end marker, see if this is the end of our grouping
					//
					x--;
					if (x <= 0)
					{
						return start;
					}
				}
				
				start++;
			}
			
			// not found
			// 
			return -1;
		}
	}
}

//+++++++++++
//EOF
//+++++++++++