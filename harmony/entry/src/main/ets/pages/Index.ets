import textcalc from 'libtextcalc.so';

interface EnterInsertion {
  shouldInsert: boolean;
  text: string;
}

@Entry
@Component
struct Index {
  @State content: string = '';

  build() {
    Column() {
      TextArea({ text: this.content })
        .width('100%')
        .height('100%')
        .fontSize(18)
        .fontFamily('monospace')
        .fontColor('#1C1F23')
        .backgroundColor('#FAFAFA')
        .padding(16)
        .onChange((value: string) => {
          this.handleChange(value);
        })
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#FAFAFA')
  }

  private handleChange(value: string): void {
    const insertion = textcalc.buildEnterInsertion(value) as EnterInsertion;
    this.content = insertion.shouldInsert ? value + insertion.text : value;
  }
}
