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
