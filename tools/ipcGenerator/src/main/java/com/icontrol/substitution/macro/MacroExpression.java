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

import org.slf4j.LoggerFactory;

import com.icontrol.substitution.Substitution;
import com.icontrol.util.StringUtils;


//-----------------------------------------------------------------------------
// Class definition:    MacroExpression
/**
 *  A macro that defines an expression to evaluate.
 *  Each has the option to attach content to it.  This is used in
 *  situations where the macro needs to pass in order to utilize 
 *  the content.  For example
 *  <pre>
 *    #if (${x})
 *       ..content of macro if pass...
 *    #elseif (${y.length} > 0)
 *       ..content of macro if pass...
 *    #else
 *       ..content of macro nothing else passed...
 *    #end
 *  </pre>
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class MacroExpression implements Expression
{
	private MacroType	type;
	private String		attachment;
	private Expression	expression;

	
	//--------------------------------------------
	/**
	 * Constructor for this class
	 */
	//--------------------------------------------
	MacroExpression(MacroType kind)
	{
		type = kind;
	}
	
	//--------------------------------------------
	/**
	 * Returns the 'command' (if, else, elseif, end)
	 */
	//--------------------------------------------
	public MacroType getType()
	{
		return type;
	}
	
	//--------------------------------------------
	/**
	 * @param type The type to set.
	 */
	//--------------------------------------------
	public void setType(MacroType type)
	{
		this.type = type;
	}

	//--------------------------------------------
	/**
	 * Return the defined attachment (i.e. the block of 
	 * code to use if this macro is successful).
	 */
	//--------------------------------------------
	public String getAttachment()
	{
		return attachment;
	}
	
	//--------------------------------------------
	/**
	 * Set the defined attachment (i.e. the block of 
	 * code to use if this macro is successful).
	 */
	//--------------------------------------------
	public void setAttachment(String content)
	{
		// #end has no content...
		//
		if (type != MacroType.END)
		{
			attachment = content;
		}
	}
	
	//--------------------------------------------
	/**
	 * 
	 */
	//--------------------------------------------
	void appendAttachment(String content)
	{
		String currAttach = getAttachment();
		if (currAttach == null)
		{
			currAttach = content;
		}
		else
		{
			currAttach += "\n" + content;
		}
		setAttachment(currAttach);
	}
	
	//--------------------------------------------
	/**
	 * Return the underlying Expression that this Macro represents
	 */
	//--------------------------------------------
	public Expression getExpression()
	{
		return expression;
	}

	//--------------------------------------------
	/**
	 * 
	 */
	//--------------------------------------------
	void setExpression(Expression exp)
	{
		expression = exp;
	}

	//--------------------------------------------
	/**
	 * @see com.icontrol.substitution.macro.Expression#evaluate(com.icontrol.substitution.Substitution)
	 */
	//--------------------------------------------
	public boolean evaluate(Substitution sub)
	{
		if (expression == null)
		{
			// depending on our 'type', this may be acceptable
			//
			if (type == MacroType.ELSE || type == MacroType.END)
			{
				return true;
			}
			
			// no expression defined
			//
			return false;
		}
		
		// log what we're evaluating
		//
//		if (logger.isDebugLevel(getClass()) == true)
//		{
//			logger.debug(this, "Evaluating macro - "+expression);
//		}
		
		// let the expression to the evaluation
		//
		boolean retVal = expression.evaluate(sub);
//		if (logger.isDebugLevel(getClass()) == true)
//		{
//			logger.debug(this, "Evaluation == "+retVal);
//		}
		
		return retVal;
	}
	
	//--------------------------------------------
	/**
	 * Used for debugging
	 * 
	 * @see java.lang.Object#toString()
	 */
	//--------------------------------------------
	@Override
	public String toString()
	{
		return "#"+type+"("+expression+")";
	}

	//--------------------------------------------
	/**
	 * Returns the value of Token.  If the Token is a
	 * 'variable', it will attempt to resolve it using
	 * the supplied Substitution object.
	 */
	//--------------------------------------------
	static String resolve(Token token, Substitution sub)
	{
		if (token == null)
		{
			return null;
		}
		
		// get the value
		//
		String value = token.getValue();
		
		// substitute if this is a 'variable'
		//
		Token.Type kind = token.getKind();
		if (kind == Token.Type.variable)
		{
			String tmp = sub.translate(value);
			return StringUtils.trimQuotes(tmp);
		}
		else if (kind == Token.Type.value)
		{
			return StringUtils.trimQuotes(value);
		}
		
		// not a type that has a value we can use
		//
		return null;
	}
	
	//--------------------------------------------
	/**
	 * Convert string to double
	 */
	//--------------------------------------------
	static double getDouble(String val)
	{
		if (val != null)
		{
			try
			{
				return Double.parseDouble(val);
			}
			catch (Exception ex)
			{
			    LoggerFactory.getLogger(MacroExpression.class).warn(ex.getMessage(), ex);
			}
		}
		return 0.0d;
	}
	
	//-----------------------------------------------------------------------------
	// Class definition:    VariableExpression
	/**
	 *  Expression with a single variable and an optional 'negate' operator
	 *  Used for situtations such as:
	 *  <pre>
	 *  	${x}
	 *  	!${x}
	 *  </pre>
	 **/
	//-----------------------------------------------------------------------------
	static class VariableExpression implements Expression
	{
		Token	variable;
		boolean	negate;

		//--------------------------------------------
		/**
		 * 
		 */
		//--------------------------------------------
		VariableExpression(Token var, boolean not)
		{
			variable = var;
			negate = not;
		}
		
		//--------------------------------------------
		/**
		 * @see com.icontrol.substitution.macro.Expression#evaluate(com.icontrol.substitution.Substitution)
		 */
		//--------------------------------------------
		public boolean evaluate(Substitution sub)
		{
			// see if 'variable' has a value
			//
			String val = MacroExpression.resolve(variable, sub);
			if (val == null || val.length() == 0)
			{
				// normally return false, but if negated then flip it
				//
				return negate;
			}
			
//			// could be a numeric expression
//			//
//			try
//			{
//				double d = Double.parseDouble(val);
//				if (d == 0.0d)
//				{
//					return negate;
//				}
//				return !negate;
//			}
//			catch (Exception fine) { }
			
			// could be a Boolean
			//
			if (val != null && val.equalsIgnoreCase("false") == true)
			{
				return negate;
			}
			
			// normally return true, but if negated then flip
			//
			return !negate;
		}
		
		//--------------------------------------------
		/**
		 * Used for debugging
		 * 
		 * @see java.lang.Object#toString()
		 */
		//--------------------------------------------
		@Override
		public String toString()
		{
			if (negate == true)
			{
				return "!"+variable.toString();
			}
			return variable.toString();
		}
	}
	
	//-----------------------------------------------------------------------------
	// Class definition:    AndExpression
	/**
	 *  Container of 2 or more Expressions that must ALL pass the evaluation
	 **/
	//-----------------------------------------------------------------------------
	static class TokenExpression implements Expression
	{
		Token			left;
		TokenComparison	compare;
		Token			right;

		//--------------------------------------------
		/**
		 * 
		 */
		//--------------------------------------------
		TokenExpression(Token left, TokenComparison compare, Token right)
		{
			this.left = left;
			this.compare = compare;
			this.right = right;
		}
		
		//--------------------------------------------
		/**
		 * @see com.icontrol.substitution.macro.Expression#evaluate(com.icontrol.substitution.Substitution)
		 */
		//--------------------------------------------
		public boolean evaluate(Substitution sub)
		{
			if (left != null && compare != null && right != null)
			{
				// compare 'left' to 'right' with 'operand'
				//
				String leftVal = resolve(left, sub);
				String rightVal = resolve(right, sub);
				
				// get the 'comparison' mechanism to use
				//
				TokenComparison.Comparison action = compare.getComparison();
				if (action == null)
				{
					// malformed
					return false;
				}
				else if (action == TokenComparison.Comparison.equals)
				{
					// equality
					//
					try
					{
						// see if both are numeric and attempt numerical equality since one 
						// could be float and one could be an integer
						//
						double leftD = Double.parseDouble(leftVal);
						double rightD = Double.parseDouble(rightVal);
						return (leftD == rightD);
					}
					catch (Exception whatever) { }
					return StringUtils.compare(leftVal, rightVal);
				}
				else if (action == TokenComparison.Comparison.not_equals)
				{
					// not-equal
					//
					try
					{
						// see if both are numeric and attempt numerical equality since one 
						// could be float and one could be an integer
						//
						double leftD = Double.parseDouble(leftVal);
						double rightD = Double.parseDouble(rightVal);
						return (leftD != rightD);
					}
					catch (Exception whatever) { }
					return !StringUtils.compare(leftVal, rightVal);
				}
				else
				{
					// numeric since doing > or <
					//
					double leftD = getDouble(leftVal);
					double rightD = getDouble(rightVal);
					
					if (action == TokenComparison.Comparison.greater_equals)
					{
						return (leftD >= rightD);
					}
					else if (action == TokenComparison.Comparison.greater)
					{
						return (leftD > rightD);
					}
					else if (action == TokenComparison.Comparison.less_equals)
					{
						return (leftD <= rightD);
					}
					else if (action == TokenComparison.Comparison.less)
					{
						return (leftD < rightD);
					}
				}
			}
			
			return false;
		}
		
		//--------------------------------------------
		/**
		 * Used for debugging
		 * 
		 * @see java.lang.Object#toString()
		 */
		//--------------------------------------------
		@Override
		public String toString()
		{
			return "(" + left.toString() + " " + compare.toString() + " " + right.toString()+")";
		}
	}
	
	//-----------------------------------------------------------------------------
	// Class definition:    AndExpression
	/**
	 *  Container of 2 Expressions that must ALL pass the evaluation
	 **/
	//-----------------------------------------------------------------------------
	static abstract class ComplexExpression implements Expression
	{
		Expression left;
		Expression right;

		//--------------------------------------------
		/**
		 * 
		 */
		//--------------------------------------------
		ComplexExpression(Expression l)
		{
			this.left = l;
		}
		
		//--------------------------------------------
		/**
		 * 
		 */
		//--------------------------------------------
		void setRight(Expression r)
		{
			this.right = r;
		}
		
	}
	
	//-----------------------------------------------------------------------------
	// Class definition:    AndExpression
	/**
	 *  Container of 2 Expressions that must ALL pass the evaluation
	 **/
	//-----------------------------------------------------------------------------
	static class AndExpression extends ComplexExpression
	{
		//--------------------------------------------
		/**
		 * 
		 */
		//--------------------------------------------
		AndExpression(Expression l)
		{
			super(l);
		}
		
		//--------------------------------------------
		/**
		 * @see com.icontrol.substitution.macro.Expression#evaluate(com.icontrol.substitution.Substitution)
		 */
		//--------------------------------------------
		public boolean evaluate(Substitution sub)
		{
			// both must pass
			//
			return (left.evaluate(sub) && right.evaluate(sub));
		}
		
		//--------------------------------------------
		/**
		 * @see java.lang.Object#toString()
		 */
		//--------------------------------------------
		@Override
		public String toString()
		{
			return left.toString() + " && " + right.toString();
		}
	}
	
	//-----------------------------------------------------------------------------
	// Class definition:    OrExpression
	/**
	 *  Container of 2 Expressions that AT-LEAST-ONE must pass the evaluation
	 **/
	//-----------------------------------------------------------------------------
	static class OrExpression extends ComplexExpression
	{
		//--------------------------------------------
		/**
		 * 
		 */
		//--------------------------------------------
		OrExpression(Expression l)
		{
			super(l);
		}

		//--------------------------------------------
		/**
		 * @see com.icontrol.substitution.macro.MacroExpression.AndExpression#evaluate(com.icontrol.substitution.Substitution)
		 */
		//--------------------------------------------
		public boolean evaluate(Substitution sub)
		{
			// either of these must pass
			//
			return (left.evaluate(sub) || right.evaluate(sub));
		}
		
		//--------------------------------------------
		/**
		 * Used for debugging
		 * 
		 * @see java.lang.Object#toString()
		 */
		//--------------------------------------------
		@Override
		public String toString()
		{
			return left.toString() + " || " + right.toString();
		}
	}
	
	//-----------------------------------------------------------------------------
	// Class definition:    NegateExpression
	/**
	 *  Expression that reverses the evaluation of another Expression.
	 *  Used for situations such as:
	 *  <pre>
	 *  	!(....)
	 *  </pre>
	 **/
	//-----------------------------------------------------------------------------
	static class NegateExpression implements Expression
	{
		Expression	inner;

		//--------------------------------------------
		/**
		 * 
		 */
		//--------------------------------------------
		NegateExpression(Expression other)
		{
			inner = other;
		}
		
		//--------------------------------------------
		/**
		 * @see com.icontrol.substitution.macro.Expression#evaluate(com.icontrol.substitution.Substitution)
		 */
		//--------------------------------------------
		public boolean evaluate(Substitution sub)
		{
			// run our inner expression
			//
			boolean success = inner.evaluate(sub);
			
			// reverse the logic
			//
			return !success;
		}
		
		//--------------------------------------------
		/**
		 * Used for debugging
		 * 
		 * @see java.lang.Object#toString()
		 */
		//--------------------------------------------
		@Override
		public String toString()
		{
			return "!"+inner.toString();
		}
	}
}

//+++++++++++
//EOF
//+++++++++++