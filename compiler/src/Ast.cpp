#include "Ast.h"
#include <stack>
#include <string>
#include "IRBuilder.h"
#include "Instruction.h"
#include "SymbolTable.h"
#include "Type.h"
#include "Unit.h"
extern Unit unit;
extern MachineUnit mUnit;

#include <iostream>

extern FILE* yyout;
int Node::counter = 0;
IRBuilder* Node::builder;

Node::Node() {
    seq = counter++;
    next = nullptr;
}
//设置下一个指针关系 即插入到末尾
void Node::setNext(Node* node) {
    Node* n = this;
    while (n->getNext()) {
        n = n->getNext();
    }
    if (n != this) {
        n->setNext(node);
    }else {
        this->next = node;
    }
}

void Node::backPatch(std::vector<Instruction*>& list, BasicBlock* bb) {
    for (auto& inst : list) {
        if (inst->isUncond())
            dynamic_cast<UncondBrInstruction*>(inst)->setBranch(bb);
        else if (inst->isCond())
            dynamic_cast<CondBrInstruction*>(inst)->setTrueBranch(bb);
    }
}

std::vector<Instruction*> Node::merge(std::vector<Instruction*>& list1, std::vector<Instruction*>& list2) {
    std::vector<Instruction*> res(list1);
    res.insert(res.end(), list2.begin(), list2.end());
    return res;
}

void Ast::genCode(Unit* unit) {
    IRBuilder* builder = new IRBuilder(unit);
    Node::setIRBuilder(builder);
    root->genCode();
}

void FunctionDef::genCode() {
    Unit* unit = builder->getUnit();
    Function* func = new Function(unit, se);
    BasicBlock* entry = func->getEntry();
    // set the insert point to the entry basicblock of this function.

    builder->setInsertBB(entry);
    if (decl)
        decl->genCode();
    // function中的stmt节点是用compoundstmt进行初始化的
    if (stmt)
        stmt->genCode();
    /**
     * Construct control flow graph. You need do set successors and predecessors
     * for each basic block. Todo
     */
    for (auto block = func->begin(); block != func->end(); block++)
    {
        //获取该块的最后一条指令
        Instruction* i = (*block)->begin();
        Instruction* last = (*block)->rbegin();
        //从块中删除条件型语句
        while (i != last) {
            if (i->isCond() || i->isUncond()) {
                (*block)->remove(i);
            }
            i = i->getNext();
        }
        
        if (last->isUncond())  //无条件跳转指令可获取跳转的目标块
        {
            BasicBlock* dst =
                dynamic_cast<UncondBrInstruction*>(last)->getBranch();
            (*block)->addSucc(dst);
            //如果无条件跳转的目标块为空 那么就插入return
            dst->addPred(*block);
            if (dst->empty()) {
                if (((FunctionType*)(se->getType()))->getRetType() ==
                    TypeSystem::intType)
                    new RetInstruction(new Operand(new ConstantSymbolEntry(
                                           TypeSystem::intType, 0)),
                                       dst);
                else if (((FunctionType*)(se->getType()))->getRetType() ==
                         TypeSystem::voidType)
                    new RetInstruction(nullptr, dst);
            }
        }
        else if (last->isCond()) {
            BasicBlock *truebranch, *falsebranch;
            truebranch =
                dynamic_cast<CondBrInstruction*>(last)->getTrueBranch();
            falsebranch =
                dynamic_cast<CondBrInstruction*>(last)->getFalseBranch();
                
            (*block)->addSucc(truebranch);
            (*block)->addSucc(falsebranch);
            truebranch->addPred(*block);
            falsebranch->addPred(*block);
        } 
        //没有显示返回或者跳转的语句 插入空返回
        else if ((!last->isRet())&&((FunctionType*)(se->getType()))->getRetType() ==TypeSystem::voidType) {
            new RetInstruction(nullptr, *block);
        }  
    }
}

BinaryExpr::BinaryExpr(SymbolEntry* se,
                       int op,
                       ExprNode* expr1,
                       ExprNode* expr2)
    : ExprNode(se), op(op), expr1(expr1), expr2(expr2) {
    dst = new Operand(se);
    std::string op_str;
    switch (op) {
        case ADD:
            op_str = "+";
            break;
        case SUB:
            op_str = "-";
            break;
        case MUL:
            op_str = "*";
            break;
        case DIV:
            op_str = "/";
            break;
        case MOD:
            op_str = "%";
            break;
        case AND:
            op_str = "&&";
            break;
        case OR:
            op_str = "||";
            break;
        case LESS:
            op_str = "<";
            break;
        case LESSEQUAL:
            op_str = "<=";
            break;
        case GREATER:
            op_str = ">";
            break;
        case GREATEREQUAL:
            op_str = ">=";
            break;
        case EQUAL:
            op_str = "==";
            break;
        case NOTEQUAL:
            op_str = "!=";
            break;
    }
 
    //对于cond需要隐式转换
    if (op >= BinaryExpr::AND && op <= BinaryExpr::NOTEQUAL)
    {
        type = TypeSystem::boolType;
        if (op == BinaryExpr::AND || op == BinaryExpr::OR) {
            if (expr1->getType()->isInt() &&
                expr1->getType()->getSize() == 32) {
                ImplictCastExpr* temp = new ImplictCastExpr(expr1);
                this->expr1 = temp;
            }
            if (expr2->getType()->isInt() &&
                expr2->getType()->getSize() == 32) {
                ImplictCastExpr* temp = new ImplictCastExpr(expr2);
                this->expr2 = temp;
            }
        }
    } 
    else
        type = TypeSystem::intType;
};

void BinaryExpr::genCode() {
    BasicBlock* bb = builder->getInsertBB();
    Function* func = bb->getParent();
    std::string op_str;
    switch (op) {
        case ADD:
            op_str = "+";
            break;
        case SUB:
            op_str = "-";
            break;
        case MUL:
            op_str = "*";
            break;
        case DIV:
            op_str = "/";
            break;
        case MOD:
            op_str = "%";
            break;
        case AND:
            op_str = "&&";
            break;
        case OR:
            op_str = "||";
            break;
        case LESS:
            op_str = "<";
            break;
        case LESSEQUAL:
            op_str = "<=";
            break;
        case GREATER:
            op_str = ">";
            break;
        case GREATEREQUAL:
            op_str = ">=";
            break;
        case EQUAL:
            op_str = "==";
            break;
        case NOTEQUAL:
            op_str = "!=";
            break;
    }
    if (op == OR) {
        // Todo
        BasicBlock* trueBB = new BasicBlock(func);
        expr1->genCode();
        backPatch(expr1->falseList(), trueBB);
        builder->setInsertBB(trueBB);
        expr2->genCode();
        true_list = merge(expr1->trueList(), expr2->trueList());
        false_list = expr2->falseList();
    }
    else if(op == AND) {
        BasicBlock* trueBB = new BasicBlock(
            func);  
        expr1->genCode();
        backPatch(expr1->trueList(), trueBB);
        builder->setInsertBB(
            trueBB);  
        expr2->genCode();
        true_list = expr2->trueList();
        false_list = merge(expr1->falseList(), expr2->falseList());
    }  
    else if (op >= ADD && op <= MOD) {
        expr1->genCode();
        expr2->genCode();
        Operand* src1 = expr1->getOperand();
        Operand* src2 = expr2->getOperand();
        int opcode = -1;
        switch (op) {
            case ADD:
                opcode = BinaryInstruction::ADD;
                break;
            case SUB:
                opcode = BinaryInstruction::SUB;
                break;
            case MUL:
                opcode = BinaryInstruction::MUL;
                break;
            case DIV:
                opcode = BinaryInstruction::DIV;
                break;
            case MOD:
                opcode = BinaryInstruction::MOD;
                break;
        }
        new BinaryInstruction(opcode, dst, src1, src2, bb);
    }
    else if (op >= LESS && op <= NOTEQUAL) {
        // Todo
        expr1->genCode();
        expr2->genCode();
        Operand* src1 = expr1->getOperand();
        Operand* src2 = expr2->getOperand();
        //0扩展 将i1转换为i32  因为关系比较都是i32
        if (src1->getType()->getSize() == 1) {
            Operand* dst = new Operand(new TemporarySymbolEntry(
                TypeSystem::intType, SymbolTable::getLabel()));
            new ZextInstruction(dst, src1, bb);
            src1 = dst;
        }
        if (src2->getType()->getSize() == 1) {
            Operand* dst = new Operand(new TemporarySymbolEntry(
                TypeSystem::intType, SymbolTable::getLabel()));
            new ZextInstruction(dst, src2, bb);
            src2 = dst;
        }
        int cmpopcode = -1;
        switch (op) {
            case LESS:
                cmpopcode = CmpInstruction::L;
                break;
            case LESSEQUAL:
                cmpopcode = CmpInstruction::LE;
                break;
            case GREATER:
                cmpopcode = CmpInstruction::G;
                break;
            case GREATEREQUAL:
                cmpopcode = CmpInstruction::GE;
                break;
            case EQUAL:
                cmpopcode = CmpInstruction::E;
                break;
            case NOTEQUAL:
                cmpopcode = CmpInstruction::NE;
                break;
        }
        new CmpInstruction(cmpopcode, dst, src1, src2, bb);
        //
        BasicBlock *truebb, *falsebb, *tempbb;
        //临时假块
        truebb = new BasicBlock(func);
        falsebb = new BasicBlock(func);
        tempbb = new BasicBlock(func);

        true_list.push_back(new CondBrInstruction(truebb, tempbb, dst, bb));

        false_list.push_back(new UncondBrInstruction(falsebb, tempbb));
    } 
}

void Constant::genCode() {
    // we don't need to generate code.
}

void Id::genCode() {
    BasicBlock* bb = builder->getInsertBB();
    Operand* addr =
        dynamic_cast<IdentifierSymbolEntry*>(symbolEntry)->getAddr();
    if (type->isInt())
        new LoadInstruction(dst, addr, bb);
    //针对数组
    //主要思想就是多维的 先把上一维度的地址找到 然后根据下标找下一个维度
    else if (type->isArray()) 
    {
        //遍历维度
        if (arrIdx) 
        {
            //获取当前类型和元素类型
            Type* type = ((ArrayType*)(this->type))->getElementType();
            Type* type1 = this->type;
            
            Operand* tempSrc = addr;//中间目标地址
            Operand* tempDst = dst;//中间目标值
            
            ExprNode* idx = arrIdx;
            //标识GepInstruction的paramFirst
            //主要是用于区分函数参数a[][3]的情况
            bool flag = false;
            
            bool pointer = false;
            bool firstFlag = true;
            
            while (true) 
            {
               //针对参数是数组的情况  a[][3]
               //把基址加载到tempSrc
                if (((ArrayType*)type1)->getLength() == -1) 
                {
                    Operand* dst1 = new Operand(new TemporarySymbolEntry(
                        new PointerType(type), SymbolTable::getLabel()));
                    tempSrc = dst1;//中间变量
                    new LoadInstruction(dst1, addr, bb);
                    
                    
                    flag = true;
                    firstFlag = false;
                }
                //如果维度遍历结束 将对应数组值传递到dst 然后退出
                if (!idx) {
                    Operand* dst1 = new Operand(new TemporarySymbolEntry(
                        new PointerType(type), SymbolTable::getLabel()));
                    Operand* idx = new Operand(
                        new ConstantSymbolEntry(TypeSystem::intType, 0));
                    new GepInstruction(dst1, tempSrc, idx, bb);
                    tempDst = dst1;
                    pointer = true;
                    break;
                }
                //生成维度
                idx->genCode();
                //用于维度寻址 将tempSrc[idx]的值加载到tempDst
                auto gep = new GepInstruction(tempDst, tempSrc,
                                              idx->getOperand(), bb, flag);
                //如果当前不是a[][3]这种情况
                //并且是第一个维度寻址
                if (!flag && firstFlag) 
                {
                    gep->setFirst();
                    firstFlag = false;
                }
                //flag每个参数都要重置
                if (flag)
                    flag = false;
                //维度要全部换成整数的维度    
                if (type == TypeSystem::intType ||
                    type == TypeSystem::constIntType)
                    break;
                type = ((ArrayType*)type)->getElementType();
                type1 = ((ArrayType*)type1)->getElementType();
                tempSrc = tempDst;
                tempDst = new Operand(new TemporarySymbolEntry(
                    new PointerType(type), SymbolTable::getLabel()));
                idx = (ExprNode*)(idx->getNext());
            }
            dst = tempDst;
            
            
            // 如果此ID是右值 需要再次load
            if (!left && !pointer) 
            {
                Operand* dst1 = new Operand(new TemporarySymbolEntry(
                    TypeSystem::intType, SymbolTable::getLabel()));
                new LoadInstruction(dst1, dst, bb);
                dst = dst1;
            }
        } 
        //针对声明数组的情况 和上面类似
        else 
        {
            if (((ArrayType*)(this->type))->getLength() == -1) 
            {
                Operand* dst1 = new Operand(new TemporarySymbolEntry(
                    new PointerType(
                        ((ArrayType*)(this->type))->getElementType()),
                    SymbolTable::getLabel()));
                new LoadInstruction(dst1, addr, bb);
                dst = dst1;

            } 
            else 
            {
                Operand* idx = new Operand(
                    new ConstantSymbolEntry(TypeSystem::intType, 0));
                auto gep = new GepInstruction(dst, addr, idx, bb);
                gep->setFirst();
            }
        }
    }
}

void IfStmt::genCode() {
    Function* func;
    BasicBlock *then_bb, *end_bb;

    func = builder->getInsertBB()->getParent();
    then_bb = new BasicBlock(func);
    end_bb = new BasicBlock(func);

    cond->genCode();

    backPatch(cond->trueList(), then_bb);
    backPatch(cond->falseList(), end_bb);

    builder->setInsertBB(then_bb);
    thenStmt->genCode();
    then_bb = builder->getInsertBB();
    new UncondBrInstruction(end_bb, then_bb);

    builder->setInsertBB(end_bb);
}

void IfElseStmt::genCode() {
    Function* func;
    BasicBlock *then_bb, *else_bb, *end_bb /*, *bb*/;
    // bb = builder->getInsertBB();
    func = builder->getInsertBB()->getParent();
    then_bb = new BasicBlock(func);
    else_bb = new BasicBlock(func);
    end_bb = new BasicBlock(func);

    cond->genCode();
    // Operand* IfElsecond = cond->getOperand();
    backPatch(cond->trueList(), then_bb);
    backPatch(cond->falseList(), else_bb);

    builder->setInsertBB(then_bb);
    thenStmt->genCode();
    then_bb = builder->getInsertBB();
    new UncondBrInstruction(end_bb, then_bb);

    builder->setInsertBB(else_bb);
    elseStmt->genCode();
    else_bb = builder->getInsertBB();
    new UncondBrInstruction(end_bb, else_bb);

    builder->setInsertBB(end_bb);
}

void CompoundStmt::genCode() {
    // Todo
    if (stmt)
        stmt->genCode();
}

void SeqNode::genCode() {
    // Todo
    stmt1->genCode();
    stmt2->genCode();
}
//已完成
void DeclStmt::genCode() {
    IdentifierSymbolEntry* se =
        dynamic_cast<IdentifierSymbolEntry*>(id->getSymbolEntry());
        //全局变量插入unit中 单独处理
    if (se->isLocal() || se->isParam()) 
    {
        //对于局部变量或者参数 
        //要先分配空间AllocaInstruction
        Function* func = builder->getInsertBB()->getParent();
        BasicBlock* entry = func->getEntry();
        Instruction* alloca;
        Operand* addr;
        SymbolEntry* addr_se;
        Type* type;
       
        type = new PointerType(se->getType());
        addr_se = new TemporarySymbolEntry(type, SymbolTable::getLabel());
        addr = new Operand(addr_se);
        alloca = new AllocaInstruction(addr, se);
        entry->insertFront(alloca); 
         //如果是参数还需要stroe
        Operand* temp = nullptr;
        if (se->isParam())
        {
            temp = se->getAddr();
            BasicBlock* bb = builder->getInsertBB();
            new StoreInstruction(addr, temp, bb);
        }
        se->setAddr(addr);  
        //如果有初始值 需要再插入store                   
        if (expr) 
        {
            BasicBlock* bb = builder->getInsertBB();
            expr->genCode();
            Operand* src = expr->getOperand();
            new StoreInstruction(addr, src, bb);
        }

    }
    else if (se->isGlobal()) {
        Operand* addr;
        SymbolEntry* addr_se;
        addr_se = new IdentifierSymbolEntry(*se);
        addr_se->setType(new PointerType(se->getType()));
        addr = new Operand(addr_se);
        se->setAddr(addr);
        unit.insertGlobal(se);
        mUnit.insertGlobal(se);
    } 
    //参数时需要使用next去下一个参数
    if (this->getNext()) {
        this->getNext()->genCode();
    }
}

void ReturnStmt::genCode() {
    // Todo
    BasicBlock* bb = builder->getInsertBB();
    Operand* src = nullptr;
    if (retValue) 
        retValue->genCode();
    if (retValue)
        src = retValue->getOperand();
    new RetInstruction(src, bb);
}

void ExprStmt::genCode() {
    // Todo
    expr->genCode();
}

void ContinueStmt::genCode() {
    // Todo
    Function* func = builder->getInsertBB()->getParent();
    BasicBlock* bb = builder->getInsertBB();
    new UncondBrInstruction(((WhileStmt*)whileStmt)->get_cond_bb(), bb);
    BasicBlock* continue_next_bb = new BasicBlock(func);
    builder->setInsertBB(continue_next_bb);
}

void BreakStmt::genCode() {
    // Todo
    Function* func = builder->getInsertBB()->getParent();
    BasicBlock* bb = builder->getInsertBB();
    new UncondBrInstruction(((WhileStmt*)whileStmt)->get_end_bb(), bb);
    BasicBlock* break_next_bb = new BasicBlock(func);
    builder->setInsertBB(break_next_bb);
}

void WhileStmt::genCode() {
    Function* func;
    BasicBlock *cond_bb, *while_bb, *end_bb, *bb;
    bb = builder->getInsertBB();
    func = builder->getInsertBB()->getParent();
    cond_bb = new BasicBlock(func);
    while_bb = new BasicBlock(func);
    end_bb = new BasicBlock(func);

    this->cond_bb = cond_bb;
    this->end_bb = end_bb;

    new UncondBrInstruction(cond_bb, bb);

    builder->setInsertBB(cond_bb);
    cond->genCode();
    backPatch(cond->trueList(), while_bb);
    backPatch(cond->falseList(), end_bb);
    
    builder->setInsertBB(while_bb);
    stmt->genCode();

    while_bb = builder->getInsertBB();
    new UncondBrInstruction(cond_bb, while_bb);

    builder->setInsertBB(end_bb);
}

void BlankStmt::genCode() {
    // Todo
}

void InitValueListExpr::genCode() {
    // Todo
}

void CallExpr::genCode() {
    std::vector<Operand*> operands;
    ExprNode* temp = param;
    while (temp) {
        temp->genCode();
        operands.push_back(temp->getOperand());
        temp = ((ExprNode*)temp->getNext());
    }
    BasicBlock* bb = builder->getInsertBB();
    new CallInstruction(dst, symbolEntry, operands, bb);
}

void UnaryExpr::genCode() {
    // Todo
    expr->genCode();
    switch (op)
    {
    case NOT:{
        BasicBlock* bb = builder->getInsertBB();
        Operand* src = expr->getOperand();
        //如果not后面是一个i32 就要先和0比较大小 然后对于结果进行取反
        if (expr->getType()->getSize() == 32) 
        {
            Operand* temp = new Operand(new TemporarySymbolEntry(
                TypeSystem::boolType, SymbolTable::getLabel()));
                
            new CmpInstruction(
                CmpInstruction::NE, temp, src,
                new Operand(new ConstantSymbolEntry(TypeSystem::intType, 0)),
                bb);
            src = temp;
        }
        new XorInstruction(dst, src, bb);
    }
        break;
    case SUB:{
        Operand* src2;
        BasicBlock* bb = builder->getInsertBB();
        Operand* src1 =
            new Operand(new ConstantSymbolEntry(TypeSystem::intType, 0));
            //i1转换为i32
        if (expr->getType()->getSize() == 1)
        {
            src2 = new Operand(new TemporarySymbolEntry(
                TypeSystem::intType, SymbolTable::getLabel()));
            new ZextInstruction(src2, expr->getOperand(), bb);
        } 
        else
            src2 = expr->getOperand();
        new BinaryInstruction(BinaryInstruction::SUB, dst, src1, src2, bb);
    }
        break;
    default:
        break;
    }
}

void ExprNode::genCode() {
    // Todo
}

bool ContinueStmt::typeCheck(Type* retType) {
    return false;
}
bool BreakStmt::typeCheck(Type* retType) {
    return false;
}
bool WhileStmt::typeCheck(Type* retType) {
    if (stmt)
        return stmt->typeCheck(retType);
    return false;
}
bool BlankStmt::typeCheck(Type* retType) {
    return false;
}
bool InitValueListExpr::typeCheck(Type* retType) {
    return false;
}
bool CallExpr::typeCheck(Type* retType) {
    return false;
}
bool UnaryExpr::typeCheck(Type* retType) {
    return false;
}

bool ExprStmt::typeCheck(Type* retType) {
    return false;
}

void AssignStmt::genCode() {
    BasicBlock* bb = builder->getInsertBB();
    expr->genCode();
    Operand* addr = nullptr;
    if (lval->getOriginType()->isInt())
        addr = dynamic_cast<IdentifierSymbolEntry*>(lval->getSymbolEntry())
                   ->getAddr();
    else if (lval->getOriginType()->isArray()) {
        ((Id*)lval)->setLeft();
        lval->genCode();
        addr = lval->getOperand();
        // Type* type = new PointerType(TypeSystem::intType);
        // SymbolEntry* addr_se = new TemporarySymbolEntry(type,
        // SymbolTable::getLabel()); addr = new Operand(addr_se);
    }
    Operand* src = expr->getOperand();
    /***
     * We haven't implemented array yet, the lval can only be ID. So we just
     * store the result of the `expr` to the addr of the id. If you want to
     * implement array, you have to caculate the address first and then store
     * the result into it.
     */
    new StoreInstruction(addr, src, bb);
}

bool Ast::typeCheck(Type* retType) {
    if (root != nullptr)
        return root->typeCheck();
    return false;
}

bool FunctionDef::typeCheck(Type* retType) {
    SymbolEntry* se = this->getSymbolEntry();
    Type* ret = ((FunctionType*)(se->getType()))->getRetType();
    StmtNode* stmt = this->stmt;
    if (stmt == nullptr) {
        if (ret != TypeSystem::voidType)
            fprintf(stderr, "非void函数不返回值\n");
        // 不嵌套函数定义就返回了
        return false;
    }
    if (!stmt->typeCheck(ret)) {
        fprintf(stderr, "函数不存在返回语句\n");
        return false;
    }
    return false;
}

bool BinaryExpr::typeCheck(Type* retType) {
    std::string op_str;
    switch (op) {
        case ADD:
            op_str = "+";
            break;
        case SUB:
            op_str = "-";
            break;
        case MUL:
            op_str = "*";
            break;
        case DIV:
            op_str = "/";
            break;
        case MOD:
            op_str = "%";
            break;
        case AND:
            op_str = "&&";
            break;
        case OR:
            op_str = "||";
            break;
        case LESS:
            op_str = "<";
            break;
        case LESSEQUAL:
            op_str = "<=";
            break;
        case GREATER:
            op_str = ">";
            break;
        case GREATEREQUAL:
            op_str = ">=";
            break;
        case EQUAL:
            op_str = "==";
            break;
        case NOTEQUAL:
            op_str = "!=";
            break;
    }
    return false;
}

bool Constant::typeCheck(Type* retType) {
    return false;
}

bool Id::typeCheck(Type* retType) {
    // 没有定义的话还是由生成树节点的时候完成吧，否则变量名拿不到
    // 重复定义也这样了
    return false;
}

bool IfStmt::typeCheck(Type* retType) {
    if (thenStmt)
        return thenStmt->typeCheck(retType);
    return false;
}

bool IfElseStmt::typeCheck(Type* retType) {
    bool flag1 = false, flag2 = false;
    if (thenStmt)
        flag1 = thenStmt->typeCheck(retType);
    if (elseStmt)
        flag2 = elseStmt->typeCheck(retType);
    return flag1 || flag2;
}

bool CompoundStmt::typeCheck(Type* retType) {
    if (stmt)
        return stmt->typeCheck(retType);
    return false;
}

bool SeqNode::typeCheck(Type* retType) {
    bool flag1 = false, flag2 = false;
    if (stmt1)
        flag1 = stmt1->typeCheck(retType);
    if (stmt2)
        flag2 = stmt2->typeCheck(retType);
    return flag1 || flag2;
}

bool DeclStmt::typeCheck(Type* retType) {
    return false;
}

bool ReturnStmt::typeCheck(Type* retType) {
    if (!retType) {
        fprintf(stderr, "应为非限定id\n");
        return true;
    }
    if (!retValue && !retType->isVoid()) {
        fprintf(
            stderr,
            "返回语句中没有值，应返回类型： \'%s\'\n",
            retType->toStr().c_str());
        return true;
    }
    if (retValue && retType->isVoid()) {
        fprintf(
            stderr,
            "返回语句中有值, 应返回类型： \'void\'\n");
        return true;
    }
    if (!retValue || !retValue->getSymbolEntry())
        return true;
    Type* type = retValue->getType();
    if (type != retType) {
        fprintf(stderr,
                "无法识别目标返回类型 \'%s\' 依据 "
                "类型 \'%s\'\n",
                retType->toStr().c_str(), type->toStr().c_str());
    }
    return true;
}

bool AssignStmt::typeCheck(Type* retType) {
    return false;
}

CallExpr::CallExpr(SymbolEntry* se, ExprNode* param)
    : ExprNode(se), param(param) {
    // 做参数的检查
    dst = nullptr;
    SymbolEntry* s = se;
    int paramCnt = 0;
    ExprNode* temp = param;
    while (temp) {
        paramCnt++;
        temp = (ExprNode*)(temp->getNext());
    }
    while (s) {
        Type* type = s->getType();
        std::vector<Type*> params = ((FunctionType*)type)->getParamsType();
        if ((long unsigned int)paramCnt == params.size()) {
            this->symbolEntry = s;
            break;
        }
        s = s->getNext();
    }
    if (symbolEntry) {
        Type* type = symbolEntry->getType();
        this->type = ((FunctionType*)type)->getRetType();
        if (this->type != TypeSystem::voidType) {
            SymbolEntry* se =
                new TemporarySymbolEntry(this->type, SymbolTable::getLabel());
            dst = new Operand(se);
        }
        std::vector<Type*> params = ((FunctionType*)type)->getParamsType();
        ExprNode* temp = param;
        for (auto it = params.begin(); it != params.end(); it++) {
            if (temp == nullptr) {
                fprintf(stderr, "函数中参数过少 %s %s\n",
                        symbolEntry->toStr().c_str(), type->toStr().c_str());
                break;
            } else if ((*it)->getKind() != temp->getType()->getKind())
                fprintf(stderr, "参数类型无法 %s 转换至 %s\n",
                        temp->getType()->toStr().c_str(),
                        (*it)->toStr().c_str());
            temp = (ExprNode*)(temp->getNext());
        }
        if (temp != nullptr) {
            fprintf(stderr, "过多参数在函数 %s %s\n",
                    symbolEntry->toStr().c_str(), type->toStr().c_str());
        }
    }
    if (((IdentifierSymbolEntry*)se)->isSysy()) {
        unit.insertDeclare(se);
    }
}

AssignStmt::AssignStmt(ExprNode* lval, ExprNode* expr)
    : lval(lval), expr(expr) {
    Type* type = ((Id*)lval)->getType();
    SymbolEntry* se = lval->getSymbolEntry();
    bool flag = true;
    if (type->isInt()) {
        if (((IntType*)type)->isConst()) {
            fprintf(stderr,
                    "无法赋值到变量 \'%s\' 使用常量限定值 "
                    "type \'%s\'\n",
                    ((IdentifierSymbolEntry*)se)->toStr().c_str(),
                    type->toStr().c_str());
            flag = false;
        }
    } else if (type->isArray()) {
        fprintf(stderr, "数组类型 \'%s\' 不可分配\n",
                type->toStr().c_str());
        flag = false;
    }
    if (flag && !expr->getType()->isInt()) {
        fprintf(stderr,
                "无法初始化变量类型 \'int\' 使用右值类型 "
                " \'%s\'\n",
                expr->getType()->toStr().c_str());
    }
}

Type* Id::getType() {
    SymbolEntry* se = this->getSymbolEntry();
    if (!se)
        return TypeSystem::voidType;
    Type* type = se->getType();
    if (!arrIdx)
        return type;
    else if (!type->isArray()) {
        fprintf(stderr, "下标值不是数组\n");
        return TypeSystem::voidType;
    } else {
        ArrayType* temp1 = (ArrayType*)type;
        ExprNode* temp2 = arrIdx;
        while (!temp1->getElementType()->isInt()) {
            if (!temp2) {
                return temp1;
            }
            temp2 = (ExprNode*)(temp2->getNext());
            temp1 = (ArrayType*)(temp1->getElementType());
        }
        if (!temp2) {
            fprintf(stderr, "下标值不是数组\n");
            return temp1;
        } else if (temp2->getNext()) {
            fprintf(stderr, "下标值不是数组\n");
            return TypeSystem::voidType;
        }
    }
    return TypeSystem::intType;
}

void ExprNode::output(int level) {
    std::string name, type;
    name = symbolEntry->toStr();
    type = symbolEntry->getType()->toStr();
    fprintf(yyout, "%*cconst string\ttype:%s\t%s\n", level, ' ', type.c_str(),
            name.c_str());
}

void Ast::output() {
    fprintf(yyout, "program\n");
    if (root != nullptr)
        root->output(4);
}

void BinaryExpr::output(int level) {
    std::string op_str;
    switch (op) {
        case ADD:
            op_str = "add";
            break;
        case SUB:
            op_str = "sub";
            break;
        case MUL:
            op_str = "mul";
            break;
        case DIV:
            op_str = "div";
            break;
        case MOD:
            op_str = "mod";
            break;
        case AND:
            op_str = "and";
            break;
        case OR:
            op_str = "or";
            break;
        case LESS:
            op_str = "less";
            break;
        case LESSEQUAL:
            op_str = "lessequal";
            break;
        case GREATER:
            op_str = "greater";
            break;
        case GREATEREQUAL:
            op_str = "greaterequal";
            break;
        case EQUAL:
            op_str = "equal";
            break;
        case NOTEQUAL:
            op_str = "notequal";
            break;
    }
    fprintf(yyout, "%*cBinaryExpr\top: %s\ttype: %s\n", level, ' ',
            op_str.c_str(), type->toStr().c_str());
    expr1->output(level + 4);
    expr2->output(level + 4);
}

int BinaryExpr::getValue() {
    int value = 0;
    switch (op) {
        case ADD:
            value = expr1->getValue() + expr2->getValue();
            break;
        case SUB:
            value = expr1->getValue() - expr2->getValue();
            break;
        case MUL:
            value = expr1->getValue() * expr2->getValue();
            break;
        case DIV:
            if(expr2->getValue())
                value = expr1->getValue() / expr2->getValue();
            break;
        case MOD:
            value = expr1->getValue() % expr2->getValue();
            break;
        case AND:
            value = expr1->getValue() && expr2->getValue();
            break;
        case OR:
            value = expr1->getValue() || expr2->getValue();
            break;
        case LESS:
            value = expr1->getValue() < expr2->getValue();
            break;
        case LESSEQUAL:
            value = expr1->getValue() <= expr2->getValue();
            break;
        case GREATER:
            value = expr1->getValue() > expr2->getValue();
            break;
        case GREATEREQUAL:
            value = expr1->getValue() >= expr2->getValue();
            break;
        case EQUAL:
            value = expr1->getValue() == expr2->getValue();
            break;
        case NOTEQUAL:
            value = expr1->getValue() != expr2->getValue();
            break;
    }
    return value;
}

UnaryExpr::UnaryExpr(SymbolEntry* se, int op, ExprNode* expr)
    : ExprNode(se, UNARYEXPR), op(op), expr(expr) {
    std::string op_str = op == UnaryExpr::NOT ? "!" : "-";
    if (expr->getType()->isVoid()) {
        fprintf(stderr,
                "void类型的操作数对一元运算符无效%s\'\n",
                op_str.c_str());
    }
    if (op == UnaryExpr::NOT) {
        type = TypeSystem::intType;
        dst = new Operand(se);
        if (expr->isUnaryExpr()) {
            UnaryExpr* ue = (UnaryExpr*)expr;
            if (ue->getOp() == UnaryExpr::NOT) {
                if (ue->getType() == TypeSystem::intType)
                    ue->setType(TypeSystem::boolType);
                // type = TypeSystem::intType;
            }
        }
        // if (expr->getType()->isInt() && expr->getType()->getSize() == 32) {
        //     ImplictCastExpr* temp = new ImplictCastExpr(expr);
        //     this->expr = temp;
        // }
    } else if (op == UnaryExpr::SUB) {
        type = TypeSystem::intType;
        dst = new Operand(se);
        if (expr->isUnaryExpr()) {
            UnaryExpr* ue = (UnaryExpr*)expr;
            if (ue->getOp() == UnaryExpr::NOT)
                if (ue->getType() == TypeSystem::intType)
                    ue->setType(TypeSystem::boolType);
        }
    }
};

void UnaryExpr::output(int level) {
    std::string op_str;
    switch (op) {
        case NOT:
            op_str = "not";
            break;
        case SUB:
            op_str = "minus";
            break;
    }
    fprintf(yyout, "%*cUnaryExpr\top: %s\ttype: %s\n", level, ' ',
            op_str.c_str(), type->toStr().c_str());
    expr->output(level + 4);
}

int UnaryExpr::getValue() {
    int value = 0;
    switch (op) {
        case NOT:
            value = !(expr->getValue());
            break;
        case SUB:
            value = -(expr->getValue());
            break;
    }
    return value;
}

void CallExpr::output(int level) {
    std::string name, type;
    int scope;
    if (symbolEntry) {
        name = symbolEntry->toStr();
        type = symbolEntry->getType()->toStr();
        scope = dynamic_cast<IdentifierSymbolEntry*>(symbolEntry)->getScope();
        fprintf(yyout, "%*cCallExpr\tfunction name: %s\tscope: %d\ttype: %s\n",
                level, ' ', name.c_str(), scope, type.c_str());
        Node* temp = param;
        while (temp) {
            temp->output(level + 4);
            temp = temp->getNext();
        }
    }
}

void Constant::output(int level) {
    std::string type, value;
    type = symbolEntry->getType()->toStr();
    value = symbolEntry->toStr();
    fprintf(yyout, "%*cIntegerLiteral\tvalue: %s\ttype: %s\n", level, ' ',
            value.c_str(), type.c_str());
}

int Constant::getValue() {
    // assert(symbolEntry->getType()->isInt());
    return ((ConstantSymbolEntry*)symbolEntry)->getValue();
}

int Id::getValue() {
    // assert(symbolEntry->getType()->isInt());
    return ((IdentifierSymbolEntry*)symbolEntry)->getValue();
}

void Id::output(int level) {
    std::string name, type;
    int scope;
    if (symbolEntry) {
        name = symbolEntry->toStr();
        type = symbolEntry->getType()->toStr();
        scope = dynamic_cast<IdentifierSymbolEntry*>(symbolEntry)->getScope();
        fprintf(yyout, "%*cId\tname: %s\tscope: %d\ttype: %s\n", level, ' ',
                name.c_str(), scope, type.c_str());
        if (arrIdx) {
            ExprNode* temp = arrIdx;
            int i = 0;
            while (temp) {
                temp->output(level + 4 + 4 * i++);
                temp = (ExprNode*)(temp->getNext());
            }
        }
    }
}

void InitValueListExpr::output(int level) {
    std::string type;
    if (symbolEntry->getType())
        type = symbolEntry->getType()->toStr();
    fprintf(yyout, "%*cInitValueListExpr\ttype: %s\n", level, ' ',
            type.c_str());
    Node* temp = expr;
    while (temp) {
        temp->output(level + 4);
        temp = temp->getNext();
    }
}

void InitValueListExpr::addExpr(ExprNode* expr) {
    if (this->expr == nullptr) {
        assert(childCnt == 0);
        childCnt++;
        this->expr = expr;
    } else {
        childCnt++;
        this->expr->setNext(expr);
    }
}

bool InitValueListExpr::isFull() {
    ArrayType* type = (ArrayType*)(this->symbolEntry->getType());
    return childCnt == type->getLength();
}

void InitValueListExpr::fill() {
    Type* type = ((ArrayType*)(this->getType()))->getElementType();
    if (type->isArray()) {
        while (!isFull())
            this->addExpr(new InitValueListExpr(new ConstantSymbolEntry(type)));
        ExprNode* temp = expr;
        while (temp) {
            ((InitValueListExpr*)temp)->fill();
            temp = (ExprNode*)(temp->getNext());
        }
    }
    if (type->isInt()) {
        while (!isFull())
            this->addExpr(new Constant(new ConstantSymbolEntry(type, 0)));
        return;
    }
}

void ImplictCastExpr::output(int level) {
    fprintf(yyout, "%*cImplictCastExpr\ttype: %s to %s\n", level, ' ',
            expr->getType()->toStr().c_str(), type->toStr().c_str());
    this->expr->output(level + 4);
}

void CompoundStmt::output(int level) {
    fprintf(yyout, "%*cCompoundStmt\n", level, ' ');
    if (stmt)
        stmt->output(level + 4);
}

void SeqNode::output(int level) {
    // fprintf(yyout, "%*cSequence\n", level, ' ');
    stmt1->output(level);
    stmt2->output(level);
}

void DeclStmt::output(int level) {
    fprintf(yyout, "%*cDeclStmt\n", level, ' ');
    id->output(level + 4);
    if (expr)
        expr->output(level + 4);
    if (this->getNext()) {
        this->getNext()->output(level);
    }
}

void BlankStmt::output(int level) {
    fprintf(yyout, "%*cBlankStmt\n", level, ' ');
}

void IfStmt::output(int level) {
    fprintf(yyout, "%*cIfStmt\n", level, ' ');
    cond->output(level + 4);
    thenStmt->output(level + 4);
}

void IfElseStmt::output(int level) {
    fprintf(yyout, "%*cIfElseStmt\n", level, ' ');
    cond->output(level + 4);
    thenStmt->output(level + 4);
    elseStmt->output(level + 4);
}

void WhileStmt::output(int level) {
    fprintf(yyout, "%*cWhileStmt\n", level, ' ');
    cond->output(level + 4);
    stmt->output(level + 4);
}
void BreakStmt::output(int level) {
    fprintf(yyout, "%*cBreakStmt\n", level, ' ');
}

void ContinueStmt::output(int level) {
    fprintf(yyout, "%*cContinueStmt\n", level, ' ');
}

void ReturnStmt::output(int level) {
    fprintf(yyout, "%*cReturnStmt\n", level, ' ');
    if (retValue != nullptr)
        retValue->output(level + 4);
}

void AssignStmt::output(int level) {
    fprintf(yyout, "%*cAssignStmt\n", level, ' ');
    lval->output(level + 4);
    expr->output(level + 4);
}

void ExprStmt::output(int level) {
    fprintf(yyout, "%*cExprStmt\n", level, ' ');
    expr->output(level + 4);
}

void FunctionDef::output(int level) {
    std::string name, type;
    name = se->toStr();
    type = se->getType()->toStr();
    fprintf(yyout, "%*cFunctionDefine\tfunction name: %s\ttype: %s\n", level,
            ' ', name.c_str(), type.c_str());
    if (decl) {
        decl->output(level + 4);
    }
    stmt->output(level + 4);
}

void ImplictCastExpr::genCode() {
    expr->genCode();
    BasicBlock* bb = builder->getInsertBB();
    Function* func = bb->getParent();
    BasicBlock* trueBB = new BasicBlock(func);
    BasicBlock* tempbb = new BasicBlock(func);
    BasicBlock* falseBB = new BasicBlock(func);

    new CmpInstruction(
        CmpInstruction::NE, this->dst, this->expr->getOperand(),
        new Operand(new ConstantSymbolEntry(TypeSystem::intType, 0)), bb);
    this->trueList().push_back(
        new CondBrInstruction(trueBB, tempbb, this->dst, bb));
    this->falseList().push_back(new UncondBrInstruction(falseBB, tempbb));
}