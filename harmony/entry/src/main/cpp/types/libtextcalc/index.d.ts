export interface EvalResult {
  ok: boolean;
  output: string;
  value: number;
}

export interface EnterInsertion {
  shouldInsert: boolean;
  text: string;
}

declare const textcalc: {
  evaluate(expression: string): EvalResult;
  buildEnterInsertion(textBeforeCaret: string): EnterInsertion;
};

export default textcalc;
