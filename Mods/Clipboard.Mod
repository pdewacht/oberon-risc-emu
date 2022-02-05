MODULE Clipboard;
  IMPORT SYSTEM, Texts, Viewers, TextFrames, Oberon;

  VAR control, data: INTEGER;

  PROCEDURE Copy(T: Texts.Text; beg, end: INTEGER);
    VAR R: Texts.Reader;
      ch: CHAR;
  BEGIN
    Texts.OpenReader(R, T, beg);
    SYSTEM.PUT(control, end - beg);
    WHILE beg < end DO
      Texts.Read(R, ch);
      SYSTEM.PUT(data, ch);
      beg := beg + 1
    END
  END Copy;

  PROCEDURE CopySelection*;
    VAR T: Texts.Text;
      beg, end, time: INTEGER;
  BEGIN
    Oberon.GetSelection(T, beg, end, time);
    IF (time >= 0) & (data # 0) THEN Copy(T, beg, end) END
  END CopySelection;

  PROCEDURE CopyViewer*;
    VAR V: Viewers.Viewer;
      F: TextFrames.Frame;
  BEGIN
    V := Oberon.MarkedViewer();
    IF (V # NIL) & (V.dsc # NIL) & (V.dsc.next IS TextFrames.Frame) & (data # 0) THEN
      F := V.dsc.next(TextFrames.Frame);
      Copy(F.text, 0, F.text.len)
    END
  END CopyViewer;

  PROCEDURE Paste*;
    VAR W: Texts.Writer;
      V: Viewers.Viewer;
      F: TextFrames.Frame;
      len, i: INTEGER;
      ch: CHAR;
  BEGIN
    V := Oberon.FocusViewer;
    IF (V # NIL) & (V.dsc # NIL) & (V.dsc.next IS TextFrames.Frame) & (data # 0) THEN
      SYSTEM.GET(control, len);
      IF len > 0 THEN
        Texts.OpenWriter(W);
        FOR i := 1 TO len DO
          SYSTEM.GET(data, ch);
          Texts.Write(W, ch)
        END;
        F := V.dsc.next(TextFrames.Frame);
        Texts.Insert(F.text, F.carloc.pos, W.buf);
        TextFrames.SetCaret(F, F.carloc.pos + len)
      END
    END
  END Paste;

BEGIN
  SYSTEM.PUT(-4, 76436C70H); (* vClp *)
  SYSTEM.GET(-4, control);
  SYSTEM.GET(-4, data)
END Clipboard.
