// Copyright Yahoo. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.schema.expressiontransforms;

import com.yahoo.searchlib.rankingexpression.evaluation.BooleanValue;
import com.yahoo.searchlib.rankingexpression.rule.ArithmeticNode;
import com.yahoo.searchlib.rankingexpression.rule.ArithmeticOperator;
import com.yahoo.searchlib.rankingexpression.rule.CompositeNode;
import com.yahoo.searchlib.rankingexpression.rule.ConstantNode;
import com.yahoo.searchlib.rankingexpression.rule.ExpressionNode;
import com.yahoo.searchlib.rankingexpression.rule.IfNode;
import com.yahoo.searchlib.rankingexpression.transform.ExpressionTransformer;
import com.yahoo.searchlib.rankingexpression.transform.TransformContext;

import java.util.ArrayDeque;
import java.util.Deque;
import java.util.Iterator;
import java.util.List;
import java.util.ArrayList;

/**
 * Transforms
 *        a &amp;&amp; b into if(a, b, false)
 * and
 *        a || b into if(a, true, b)
 * to avoid computing b if a is false and true respectively.
 *
 * This may increase performance since boolean expressions are not short-circuited.
 *
 * @author bratseth
 */
public class BooleanExpressionTransformer extends ExpressionTransformer<TransformContext> {

    @Override
    public ExpressionNode transform(ExpressionNode node, TransformContext context) {
        if (node instanceof CompositeNode composite)
            node = transformChildren(composite, context);

        if (node instanceof ArithmeticNode arithmetic)
            node = transformBooleanArithmetics(arithmetic);

        return node;
    }

    private ExpressionNode transformBooleanArithmetics(ArithmeticNode node) {
        Iterator<ExpressionNode> child = node.children().iterator();

        // Transform in precedence order:
        Deque<ChildNode> stack = new ArrayDeque<>();
        stack.push(new ChildNode(null, child.next()));
        for (Iterator<ArithmeticOperator> it = node.operators().iterator(); it.hasNext() && child.hasNext();) {
            ArithmeticOperator op = it.next();
            if ( ! stack.isEmpty()) {
                while (stack.size() > 1 && ! op.hasPrecedenceOver(stack.peek().op)) {
                    popStack(stack);
                }
            }
            stack.push(new ChildNode(op, child.next()));
        }
        while (stack.size() > 1)
            popStack(stack);
        return stack.getFirst().child;
    }

    private void popStack(Deque<ChildNode> stack) {
        ChildNode rhs = stack.pop();
        ChildNode lhs = stack.peek();

        ExpressionNode combination;
        if (rhs.op == ArithmeticOperator.AND)
            combination = andByIfNode(lhs.child, rhs.child);
        else if (rhs.op == ArithmeticOperator.OR)
            combination = orByIfNode(lhs.child, rhs.child);
        else {
            combination = resolve(lhs, rhs);
            lhs.artificial = true;
        }
        lhs.child = combination;
    }

    private static ArithmeticNode resolve(ChildNode left, ChildNode right) {
        if ( ! (left.child instanceof ArithmeticNode) && ! (right.child instanceof ArithmeticNode))
            return new ArithmeticNode(left.child, right.op, right.child);

        // Collapse inserted ArithmeticNodes
        List<ArithmeticOperator> joinedOps = new ArrayList<>();
        joinOps(left, joinedOps);
        joinedOps.add(right.op);
        joinOps(right, joinedOps);
        List<ExpressionNode> joinedChildren = new ArrayList<>();
        joinChildren(left, joinedChildren);
        joinChildren(right, joinedChildren);
        return new ArithmeticNode(joinedChildren, joinedOps);
    }

    private static void joinOps(ChildNode node, List<ArithmeticOperator> joinedOps) {
        if (node.artificial && node.child instanceof ArithmeticNode arithmeticNode)
            joinedOps.addAll(arithmeticNode.operators());
    }
    private static void joinChildren(ChildNode node, List<ExpressionNode> joinedChildren) {
        if (node.artificial && node.child instanceof ArithmeticNode arithmeticNode)
            joinedChildren.addAll(arithmeticNode.children());
        else
            joinedChildren.add(node.child);
    }


    private IfNode andByIfNode(ExpressionNode a, ExpressionNode b) {
        return new IfNode(a, b, new ConstantNode(new BooleanValue(false)));
    }

    private IfNode orByIfNode(ExpressionNode a, ExpressionNode b) {
        return new IfNode(a, new ConstantNode(new BooleanValue(true)), b);
    }

    /** A child with the operator to be applied to it when combining it with the previous child. */
    private static class ChildNode {

        final ArithmeticOperator op;
        ExpressionNode child;
        boolean artificial;

        public ChildNode(ArithmeticOperator op, ExpressionNode child) {
            this.op = op;
            this.child = child;
        }

        @Override
        public String toString() {
            return child.toString();
        }

    }

}
