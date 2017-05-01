#include "deluxe.h"
#include "d68test.h"

TEST_F(DeluxeTest, Basic)
{
  EXPECT_EQ("", xform(""));
}

TEST_F(DeluxeTest, NoRegs)
{
  EXPECT_EQ(
        "foo:\n"
        "\t\trts\n",
      //---------------------
      xform(
        "\t\t@proc foo\n"
        "\t\t@endproc foo\n"));
}

TEST_F(DeluxeTest, OneReg)
{
  EXPECT_EQ(
        "foo:\n"
        "\t\tmovem.l d7,-(sp)\n"
        "\t\tmovem.l (sp)+,d7\n"
        "\t\trts\n",
      //---------------------
      xform(
        "\t\t@proc foo\n"
        "\t\t@dreg a\n"
        "\t\t@endproc\n"));
}

TEST_F(DeluxeTest, MultiReg)
{
  EXPECT_EQ(
        "foo:\n"
        "\t\tmovem.l d7/a5/a6,-(sp)\n"
        "\t\tmovem.l (sp)+,d7/a5/a6\n"
        "\t\trts\n",
      //---------------------
      xform(
        "\t\t@proc foo\n"
        "\t\t@dreg a\n"
        "\t\t@areg b,c\n"
        "\t\t@endproc\n"));
}

TEST_F(DeluxeTest, SpillSaves)
{
  EXPECT_EQ(xform(
        "\t\t@proc foo\n"
        "\t\t@spill d0\n"
        "\t\t@restore d0\n"
        "\t\t@endproc\n"),
      //---------------------
        "foo:\n"
        "\t\tmovem.l d0,-(sp)\n"
        "\t\tmovem.l (sp)+,d0\n"
        "\t\trts\n");
}

TEST_F(DeluxeTest, StackSlots)
{
  EXPECT_EQ(
        "foo:\n"
        "\t\tmovem.l d6/d7,-(sp)\n"
        "\t\tmove.l #$aaaaaaaa,d7\n"
        "\t\tmove.l #$bbbbbbbb,d6\n"
        "\t\tmovem.l d6/d7,-(sp)\n"
        "\t\tmove.l 4(sp),d7\n"
        "\t\tmove.l 0(sp),d6\n"
        "\t\tbsr ExternalProc\n"
        "\t\tmovem.l (sp)+,d6/d7\n"
        "\t\tmovem.l (sp)+,d6/d7\n"
        "\t\trts\n",
      //---------------------
      xform(
        "\t\t@proc foo\n"
        "\t\t@dreg a,b\n"
        "\t\tmove.l #$aaaaaaaa,@a\n"
        "\t\tmove.l #$bbbbbbbb,@b\n"
        "\t\t@spill d6,d7\n"
        "\t\tmove.l @a,d7\n"
        "\t\tmove.l @b,d6\n"
        "\t\tbsr ExternalProc\n"
        "\t\t@restore d7,d6\n"
        "\t\t@endproc\n"));
}

TEST_F(DeluxeTest, StackSlots2)
{
  EXPECT_EQ(
        "foo:\n"
        "\t\tmovem.l d6/d7,-(sp)\n"
        "\t\tmove.l #$aaaaaaaa,d7\n"
        "\t\tmove.l #$bbbbbbbb,d6\n"
        "\t\tmovem.l d6/d7,-(sp)\n"
        "\t\tmove.l 4(sp),d7\n"
        "\t\tmove.l 0(sp),d6\n"
        "\t\tbsr ExternalProc\n"
        "\t\tmovem.l (sp)+,d6/d7\n"
        "\t\tmovem.l (sp)+,d6/d7\n"
        "\t\trts\n",
      //---------------------
      xform(
        "\t\t@proc foo\n"
        "\t\t@dreg a,b\n"
        "\t\tmove.l #$aaaaaaaa,@a\n"
        "\t\tmove.l #$bbbbbbbb,@b\n"
        "\t\t@spill d7,d6\n"  // Make sure spill d6,d7 and d7,d6 produce identical results as above StackSlots test
        "\t\tmove.l @a,d7\n"
        "\t\tmove.l @b,d6\n"
        "\t\tbsr ExternalProc\n"
        "\t\t@restore d7,d6\n"
        "\t\t@endproc\n"));
}

